#include "servo.h"

#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"
#include "output.h"

#if defined(STM32H7)
#include "stm32h7xx_hal.h"
#elif defined(STM32F1)
#include "stm32f1xx_hal.h"
#else
#error "servo.c needs a HAL include for this chip family"
#endif

#if HELM_FEATURE_PARAMS_PERSIST
#include "params.h"
#endif

/* Defined in src/main.c, called on an unrecoverable HAL init failure --
   same pattern board.c's own HAL init calls already use, just with an
   explicit declaration here rather than relying on an implicit one
   (board.c's own call sites predate this file and still warn on that;
   out of scope to fix there for this issue). */
extern void Error_Handler(void);

/* See mapping.c's own comment -- same placeholder-period-and-priority
   reasoning applies to every stage in this chain (issue #7). Real timing
   requirement now exists one layer down (the PWM hardware itself runs
   its own 50Hz frame autonomously, independent of this task's period) --
   this task's job is just to read output.c's latest values and write
   them into CCR often enough that a stale command doesn't linger,
   comfortably faster than a hobby servo can physically respond to a
   changed command anyway (control-loops.md's own "full-scale move in
   ~2.5ms" note). */
#define SERVO_TASK_PERIOD_MS 20
#define SERVO_TASK_PRIORITY 1

/* Issue #31's own spec: 50Hz frame (the standard hobby PWM servo rate) by
   default, or 250Hz/333Hz if PARAM_SERVO_RATE says so (servo.h's own
   comment on SERVO_RATE_50HZ/_250HZ/_333HZ) -- either way, microsecond
   pulse width via ARR/CCR. servoPeriodUs is resolved once in
   servo_start(), before servo_hw_init() ever runs, into whichever of
   these this boot actually uses. SERVO_PWM_SAFE_CENTER_US is the pulse
   commanded before any real output.c value has ever been written (both
   HAL_TIM_PWM_ConfigChannel()'s initial Pulse and this driver's own
   fallback/seed values) -- a neutral position, not an arbitrary number,
   and independent of which frame rate is active. */
#define SERVO_PWM_PERIOD_50HZ_US 20000U
#define SERVO_PWM_PERIOD_250HZ_US 4000U /* 1/250Hz = 4000us exactly, no rounding
                                            needed unlike 333Hz below */
#define SERVO_PWM_PERIOD_333HZ_US 3000U /* 1/333Hz ~= 3003us; digital-servo specs
                                            themselves express this as a flat 3ms
                                            frame, not a precise Hz figure -- follow
                                            that convention rather than rounding
                                            differently here */
#define SERVO_PWM_SAFE_CENTER_US 1500U

static uint32_t servoPeriodUs;

/* Exactly as many distinct physical timers as issue #31's own pin table
   needs: matek_h743 TIM4+TIM5, afroflight32 TIM1+TIM4. Bump this (and
   double-check enable_timer_clock()/timer_clock_hz() below) before ever
   adding a board whose table needs a third. */
#define SERVO_MAX_TIMERS 2

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t alternate; /* unused on STM32F1 -- that family's GPIO_InitTypeDef
                            has no Alternate field at all (older AFIO-remap
                            model, not per-pin AF muxing); kept in every
                            board's table regardless so one struct shape
                            works for both, rather than diverging the type
                            itself per chip family. */
    TIM_TypeDef *timer;
    uint32_t channel;
} ServoPadConfig;

/* Issue #31's own pin/timer facts, ported from aoa-boat-controller's pin
   headers -- re-verify against the physical boards before trusting
   blindly, same as every other pin fact in this project.

   matek_h743: Output3 (PA0/TIM5_CH1) is bench-confirmed against this
   exact unit's live `resource` output; Output4-10 are sourced from two
   independent firmware configs (ArduPilot + Betaflight) agreeing, not
   independently bench-confirmed. PA0-PA3 = TIM5_CH1-4, PD12-PD15 =
   TIM4_CH1-4, both AF2 -- standard H7 alternate-function pinout for
   these two timers.

   One TIM_HandleTypeDef per distinct TIMx, shared across every channel
   registered on it -- NOT one per channel (see servo_hw_init()'s own
   comment for why this is a real bug class, not a theoretical one):
   S3-S6 all share TIM5, S7-S10 all share TIM4. */
#if defined(STM32H7)
static const ServoPadConfig padConfigs[HELM_SERVO_COUNT] = {
    {GPIOA, GPIO_PIN_0, GPIO_AF2_TIM5, TIM5, TIM_CHANNEL_1},  /* S3 */
    {GPIOA, GPIO_PIN_1, GPIO_AF2_TIM5, TIM5, TIM_CHANNEL_2},  /* S4 */
    {GPIOA, GPIO_PIN_2, GPIO_AF2_TIM5, TIM5, TIM_CHANNEL_3},  /* S5 */
    {GPIOA, GPIO_PIN_3, GPIO_AF2_TIM5, TIM5, TIM_CHANNEL_4},  /* S6 */
    {GPIOD, GPIO_PIN_12, GPIO_AF2_TIM4, TIM4, TIM_CHANNEL_1}, /* S7 */
    {GPIOD, GPIO_PIN_13, GPIO_AF2_TIM4, TIM4, TIM_CHANNEL_2}, /* S8 */
    {GPIOD, GPIO_PIN_14, GPIO_AF2_TIM4, TIM4, TIM_CHANNEL_3}, /* S9 */
    {GPIOD, GPIO_PIN_15, GPIO_AF2_TIM4, TIM4, TIM_CHANNEL_4}, /* S10 */
};
#elif defined(STM32F1)
/* afroflight32: OUT1-6 (PA8/PA11/PB6-PB9) are cleanflight target.c's pad
   list for this board -- populated servo connectors, not independently
   bench-confirmed on this project's own unit yet. Default (non-remapped)
   TIM1/TIM4 pins for the F103 -- no AFIO_REMAP needed for these, unlike
   system_clock_config()'s own SWJ-NOJTAG remap for the debug pins.
   OUT1/OUT2 share TIM1, OUT3-6 all share TIM4. */
static const ServoPadConfig padConfigs[HELM_SERVO_COUNT] = {
    {GPIOA, GPIO_PIN_8, 0, TIM1, TIM_CHANNEL_1},  /* OUT1 */
    {GPIOA, GPIO_PIN_11, 0, TIM1, TIM_CHANNEL_4}, /* OUT2 */
    {GPIOB, GPIO_PIN_6, 0, TIM4, TIM_CHANNEL_1},  /* OUT3 */
    {GPIOB, GPIO_PIN_7, 0, TIM4, TIM_CHANNEL_2},  /* OUT4 */
    {GPIOB, GPIO_PIN_8, 0, TIM4, TIM_CHANNEL_3},  /* OUT5 */
    {GPIOB, GPIO_PIN_9, 0, TIM4, TIM_CHANNEL_4},  /* OUT6 */
};
#else
#error "servo.c needs a padConfigs[] table for this chip family -- see matek_h743/afroflight32's own for the shape"
#endif

typedef struct {
    TIM_TypeDef *instance;
    TIM_HandleTypeDef handle;
} ServoTimer;

/* Static storage -- zero-initialized at program start (BSS), same as
   every other module's file-scope state in this project. Each handle's
   unset fields (e.g. RepetitionCounter, meaningless for TIM4/TIM5 but
   real for TIM1's advanced-timer shape) start correctly at 0 this way,
   not left uninitialized. */
static ServoTimer servoTimers[SERVO_MAX_TIMERS];
static uint8_t servoTimerCount;
static TIM_HandleTypeDef *slotTimerHandle[HELM_SERVO_COUNT];

static QueueHandle_t servo_queue;

/* One macro per distinct timer this driver ever touches (padConfigs[]
   above) -- same RCC enable macro names on both chip families' HAL
   packages, so this doesn't need its own #if defined() split. An
   instance outside this known set is a real bug (padConfigs[] naming a
   timer this function doesn't know about), not something to silently
   ignore. */
static void enable_timer_clock(TIM_TypeDef *instance) {
    if (instance == TIM4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
#if defined(STM32H7)
    } else if (instance == TIM5) {
        __HAL_RCC_TIM5_CLK_ENABLE();
#elif defined(STM32F1)
    } else if (instance == TIM1) {
        __HAL_RCC_TIM1_CLK_ENABLE();
#endif
    } else {
        Error_Handler();
    }
}

/* One macro per distinct GPIO port padConfigs[] above actually uses --
   same reasoning/shape as enable_timer_clock() above. __HAL_RCC_GPIOx_
   CLK_ENABLE() is safe to call more than once (multiple slots sharing a
   port, e.g. PD12-15 all on GPIOD) -- it only sets an RCC enable bit,
   same as every other board_*_init() in this project already relies on
   implicitly by calling these macros unconditionally per pin. */
static void enable_gpio_port_clock(GPIO_TypeDef *port) {
    if (port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
#if defined(STM32H7)
    } else if (port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
#elif defined(STM32F1)
    } else if (port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
#endif
    } else {
        Error_Handler();
    }
}

/* The real timer clock this specific instance runs at, right now, on
   this specific chip -- NOT a compile-time constant on matek_h743:
   board.c's own system_clock_config() picks PLLN based on a runtime
   silicon-revision check (Rev V: 480MHz sysclk -> 240MHz APB1 timer
   clock; older: 400MHz sysclk -> 200MHz), so the real value can only be
   known at runtime, not assumed at compile time. HAL_RCC_GetPCLK1Freq()
   reflects whichever revision this exact chip actually is; the *2 is
   this project's own clock config's APB1 prescaler (RCC_APB1_DIV2,
   board.c), which per the standard STM32 rule doubles the TIMER clock
   specifically (not the peripheral bus clock reported by
   HAL_RCC_GetPCLK1Freq() itself) whenever the APBx prescaler is >1 --
   both TIM4 and TIM5 are APB1 timers on this chip.

   afroflight32's clock is fully fixed (board.c's own comment: 72MHz
   HSEx6 PLL, no silicon-revision ambiguity), so 72MHz is a safe
   compile-time constant there for both TIM1 (APB2 div1, no doubling:
   72MHz) and TIM4 (APB1 div2, doubled: 36MHz*2=72MHz) -- the same final
   number for both is a coincidence of this exact board's specific
   dividers, not a general rule to assume elsewhere. */
static uint32_t timer_clock_hz(TIM_TypeDef *instance) {
    (void)instance;
#if defined(STM32H7)
    return HAL_RCC_GetPCLK1Freq() * 2UL;
#elif defined(STM32F1)
    return 72000000UL;
#endif
}

/* Finds this timer instance's already-initialized handle, or creates and
   HAL_TIM_PWM_Init()s it the first time any slot needs it -- exactly one
   TIM_HandleTypeDef per distinct TIMx, shared across every channel
   registered on it, never one per channel. This is a real bug class, not
   a theoretical one: aoa-boat-controller's own predecessor (one
   ServoOutput instance per pad) hit exactly this and was retired for it
   (issue #31's own body has the full account) -- constructing a second
   handle for a timer that already has channels configured would
   re-run HAL_TIM_PWM_Init() and silently reset state the first handle's
   channels depend on, corrupting already-configured channels on the same
   physical timer. */
static TIM_HandleTypeDef *find_or_create_timer(TIM_TypeDef *instance) {
    for (uint8_t i = 0; i < servoTimerCount; i++) {
        if (servoTimers[i].instance == instance) {
            return &servoTimers[i].handle;
        }
    }

    if (servoTimerCount >= SERVO_MAX_TIMERS) {
        /* padConfigs[] names more distinct timers than SERVO_MAX_TIMERS
           accounts for -- a real bug (this project's own two boards both
           need exactly 2), fail loud rather than silently reuse the
           wrong handle. */
        Error_Handler();
    }

    ServoTimer *timer = &servoTimers[servoTimerCount++];
    timer->instance = instance;

    enable_timer_clock(instance);

    uint32_t const prescaler = (timer_clock_hz(instance) / 1000000UL) - 1UL; /* 1 tick == 1us */

    timer->handle.Instance = instance;
    timer->handle.Init.Prescaler = prescaler;
    timer->handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    timer->handle.Init.Period = servoPeriodUs - 1U; /* one persisted setting for
                                                         every timer this boot,
                                                         resolved in servo_start()
                                                         before this ever runs --
                                                         period is a per-timer
                                                         property, not per-channel */
    timer->handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    timer->handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&timer->handle) != HAL_OK) {
        Error_Handler();
    }

    return &timer->handle;
}

/* Configures one physical pad's GPIO + PWM channel (a safe centered
   pulse, not yet running -- HAL_TIM_PWM_Start() happens separately, once
   every slot's channel is configured, per issue #31's own ordering
   requirement below). Does NOT start the timer/channel itself. */
static void configure_slot(uint8_t slot) {
    const ServoPadConfig *pad = &padConfigs[slot];

    enable_gpio_port_clock(pad->port);

    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = pad->pin;
    gpioInit.Mode = GPIO_MODE_AF_PP;
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW; /* a hobby servo signal has no fast-edge
                                              requirement -- same choice this
                                              project's other AF pins already
                                              make (e.g. board.c's own USART6/
                                              UART7 pin config) */
#if defined(STM32H7)
    gpioInit.Alternate = pad->alternate;
#endif
    HAL_GPIO_Init(pad->port, &gpioInit);

    TIM_HandleTypeDef *timerHandle = find_or_create_timer(pad->timer);
    slotTimerHandle[slot] = timerHandle;

    TIM_OC_InitTypeDef ocInit = {0};
    ocInit.OCMode = TIM_OCMODE_PWM1;
    ocInit.Pulse = SERVO_PWM_SAFE_CENTER_US;
    ocInit.OCPolarity = TIM_OCPOLARITY_HIGH;
    ocInit.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(timerHandle, &ocInit, pad->channel) != HAL_OK) {
        Error_Handler();
    }
}

/* Issue #31's own two-pass ordering requirement: configure every
   channel's PWM mode + safe centered pulse FIRST, then start each
   distinct timer's period/overflow -- not interleaved per-slot. A
   single interleaved pass would call HAL_TIM_PWM_Start() on a timer
   after only SOME of its channels were configured (e.g. TIM5's CH1
   started before CH2-4 exist yet), which works today only by accident
   of timer/channel enable being independent bits -- keeping the two
   passes separate is what issue #31's own text calls out explicitly,
   not an accident to preserve. */
static void servo_hw_init(void) {
    for (uint8_t i = 0; i < HELM_SERVO_COUNT; i++) {
        configure_slot(i);
    }
    for (uint8_t i = 0; i < HELM_SERVO_COUNT; i++) {
        if (HAL_TIM_PWM_Start(slotTimerHandle[i], padConfigs[i].channel) != HAL_OK) {
            Error_Handler();
        }
    }
}

static void servo_hw_write(uint8_t slot, uint16_t pulse_us) {
    __HAL_TIM_SET_COMPARE(slotTimerHandle[slot], padConfigs[slot].channel, pulse_us);
}

static void servo_task(void *arg) {
    (void)arg;

    ServoFrame fallback = {0};
    fallback.status = RX_STATUS_FAILSAFE;
    for (uint8_t i = 0; i < HELM_SERVO_COUNT; i++) {
        fallback.servos[i] = SERVO_PWM_SAFE_CENTER_US;
    }
    SupervisorHandle handle = supervisor_register("servo", servo_queue, &fallback,
                                                   sizeof(fallback),
                                                   pdMS_TO_TICKS(SERVO_TASK_PERIOD_MS * 3));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(SERVO_TASK_PERIOD_MS));

        OutputFrame in;
        output_get_latest(&in);

        ServoFrame out;
        memcpy(out.servos, in.servos, sizeof(out.servos));
        out.status = in.status;

        for (uint8_t i = 0; i < HELM_SERVO_COUNT; i++) {
            servo_hw_write(i, out.servos[i]);
        }

        xQueueOverwrite(servo_queue, &out);
        supervisor_kick(handle);
    }
}

void servo_start(void) {
    /* Resolve the PWM frame rate before servo_hw_init() ever touches a
       timer -- one persisted setting for every timer this boot (servo.h's
       own comment on why a single param is the only shape that's ever
       valid here). Same "persisted override, compile-time-safe fallback,
       read once at container-start" shape issue #10 already established
       for PARAM_INPUT_MODE. */
#if HELM_FEATURE_PARAMS_PERSIST
    uint32_t servoRate = SERVO_RATE_50HZ;
    param_get_u32(PARAM_SERVO_RATE, &servoRate);
    if (servoRate == SERVO_RATE_333HZ) {
        servoPeriodUs = SERVO_PWM_PERIOD_333HZ_US;
    } else if (servoRate == SERVO_RATE_250HZ) {
        servoPeriodUs = SERVO_PWM_PERIOD_250HZ_US;
    } else {
        /* SERVO_RATE_50HZ, or any value outside the known set (including
           the deliberately-unassigned 2) -- fail safe to the rate every
           servo tolerates, not undefined behavior on a bad param. */
        servoPeriodUs = SERVO_PWM_PERIOD_50HZ_US;
    }
#else
    servoPeriodUs = SERVO_PWM_PERIOD_50HZ_US;
#endif

    servo_queue = xQueueCreate(1, sizeof(ServoFrame));

    /* Seed the queue before anything downstream can peek it, same as
       every other module -- servo.h has no downstream consumer of its
       own today, but diag.c's diag_pipeline() does. */
    ServoFrame initial = {0};
    initial.status = RX_STATUS_FAILSAFE;
    for (uint8_t i = 0; i < HELM_SERVO_COUNT; i++) {
        initial.servos[i] = SERVO_PWM_SAFE_CENTER_US;
    }
    xQueueOverwrite(servo_queue, &initial);

    servo_hw_init();

    xTaskCreate(servo_task, "servo", configMINIMAL_STACK_SIZE, NULL,
                SERVO_TASK_PRIORITY, NULL);
}

void servo_get_latest(ServoFrame *out) {
    xQueuePeek(servo_queue, out, 0);
}
