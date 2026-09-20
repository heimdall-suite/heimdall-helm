#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "board_features.h"
#include "control.h"
#include "heartbeat.h"
#include "mapping.h"
#include "output.h"
#include "rx.h"
#include "servo.h"
#include "supervisor.h"
#include "telemetry.h"

#if defined(STM32H7)
#include "stm32h7xx_hal.h"
#elif defined(STM32F7)
#include "stm32f7xx_hal.h"
#elif defined(STM32F1)
#include "stm32f1xx_hal.h"
#else
#error "Unknown MCU family -- add its HAL include here for this board."
#endif

#if HELM_HAS_ROM_BOOTLOADER_JUMP
#include "bootloader.h"
#endif
#if HELM_FEATURE_CLI
#include "cli.h"
#endif
#if HELM_FEATURE_PARAMS_PERSIST
#include "params.h"
#endif
#if HELM_FEATURE_TELEMETRY_SPORT && HELM_HAS_SPORT_UART
#include "sport.h"
#endif
#if HELM_HAS_IMU
#include "imu.h"
#endif
#if HELM_HAS_BARO
#include "baro.h"
#endif

extern void xPortSysTickHandler(void);

/* Called by boards/<target>/board.c on an unrecoverable HAL clock/init
   failure -- before the scheduler exists, so there's no task to fail
   gracefully into. Halts rather than continuing on unconfirmed clocks. */
void Error_Handler(void) {
    __disable_irq();
    for (;;) {
    }
}

/* FreeRTOS hook: called from a task's own stack if pvPortMalloc() can't
   satisfy an allocation. Halts rather than continuing on a heap that's
   already known to be exhausted. */
void vApplicationMallocFailedHook(void) {
    __disable_irq();
    for (;;) {
    }
}

/* FreeRTOS hook: called if a task's stack overflows (requires
   configCHECK_FOR_STACK_OVERFLOW). Halts rather than continuing with
   corrupted memory below the stack. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    __disable_irq();
    for (;;) {
    }
}

/* CMSIS SysTick interrupt handler. Always feeds HAL's own millisecond
   tick (HAL_GetTick() depends on it, e.g. for HAL_Delay() during
   board_init() before the scheduler exists), and additionally drives
   FreeRTOS's tick once the scheduler has actually started. */
void SysTick_Handler(void) {
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

/* Composition root shared by every board -- board-specific init lives in
   boards/<target>/board.c, not here. Real modules get started here, each
   gated on board_features.h's HELM_FEATURE_* flags, so a board that
   doesn't budget for a feature (see boards/afroflight32/board_features.h)
   simply never starts that module's task, rather than every module
   having its own scattered per-board #ifdefs. */
int main(void) {
#if HELM_HAS_ROM_BOOTLOADER_JUMP
    // Check whether the CLI's `dfu` command left a reboot-into-bootloader
    // request behind; if so, jump straight into it and never return.
    // Literal first statement -- before HAL_Init()/board_init() touch any
    // clock or peripheral, see bootloader_jump_if_requested()'s own comment.
    // Gated on HELM_HAS_ROM_BOOTLOADER_JUMP, not HELM_FEATURE_CLI -- issue
    // #13 fixed this after finding it would otherwise fail to link on a
    // board with a CLI but no bootloader-jump port (lib/bootloader.h's own
    // header comment already documented this as the correct gate; this
    // call site and cli.c's cmd_dfu registration just weren't following it
    // yet).
    bootloader_jump_if_requested();
#endif

#if defined(STM32F1)
    // This board's ROM bootloader is entered with BOOT0 strapped high,
    // which aliases address 0 to system memory (the bootloader's own
    // vector table) for the CPU's whole session -- that aliasing, and
    // SCB->VTOR itself, are only re-sampled/reset on an actual reset,
    // never touched by the bootloader's raw "Go" jump into our code
    // (just a PC/SP load, not a reset). CMSIS's own SystemInit()
    // (system_stm32f1xx.c) never sets VTOR either unless
    // USER_VECT_TAB_ADDRESS is defined, which it isn't by default. Net
    // effect: our code runs fine at first (a HardFault-free jump doesn't
    // need VTOR), but ANY actual interrupt -- including SysTick, which
    // HAL_Delay() and every FreeRTOS task delay depend on -- vectors
    // into the STALE bootloader table instead of ours, since only an
    // actual reset (not this software jump) re-samples BOOT0 and
    // updates the address-0 alias VTOR effectively reads through.
    // Bench-confirmed on this exact board during #13's bring-up: without
    // this line, execution ran fine right up to the first place
    // anything depended on a real interrupt firing, then silently hung
    // forever. Fixing this here, as early as possible, before anything
    // relies on an interrupt firing correctly.
    SCB->VTOR = FLASH_BASE;
#endif

    // Bring up HAL's own tick/timebase, then this board's clock tree and
    // any other early peripheral init board.c owns.
    HAL_Init();
    board_init();

    // Start the module-liveness supervisor (issue #4) before any module
    // that will register with it (.docs/architecture/module-architecture.md's
    // "Fault isolation" section) -- unconditional, not gated behind a
    // HELM_FEATURE_* flag, same as the heartbeat below: every board gets
    // the safety net regardless of feature budget. On boards with
    // HELM_HAS_IWDG set (issue #5), the supervisor task also owns starting
    // and feeding the independent hardware watchdog from its own healthy
    // pass -- see supervisor.c. A wedged supervisor, or the crash hooks
    // below halting with interrupts disabled, both stop that feed and let
    // IWDG reset the MCU; board_init() is responsible for a safe power-on
    // state.
    supervisor_start();

#if HELM_FEATURE_PARAMS_PERSIST
    // Load the persisted-param store (issue #32) before anything that
    // will eventually read from it -- currently just proves itself via
    // the CLI's `param` command (no real consumer wired up yet); #10's
    // rx_start() input-mode binding is the reason this call sits before
    // the pipeline below, not just alphabetical convenience. Synchronous,
    // not a task: reads a fixed-size flash region into a RAM cache once,
    // same "direct call before the scheduler starts" shape as board_init()
    // above, not a _start()-a-task module like the ones below it.
    params_init();
#endif

    // Start the Input->Mapping->Control->Output->Servo stub chain (issue
    // #7), each stage its own task/queue/supervisor-registered module
    // per .docs/architecture/module-architecture.md, wired in this
    // pipeline order (though that order isn't actually load-bearing: every
    // module's queue is created and seeded here, before
    // vTaskStartScheduler(), so no consumer can run before a producer's
    // queue holds a valid value regardless of _start() call order).
    // rx_start()'s driver pick is board_features.h's compile-time
    // HELM_RX_DEFAULT_PROTOCOL_SBUS/_CRSF default (issue #6); every stage
    // past it is a fixed passthrough stand-in, no real mapping table or
    // control-loop math yet -- see each module's own header. This
    // #include chain is also what pulls lib/rx/ (and these new libs) into
    // the build via the LDF's normal chain scan -- see platformio.ini's
    // header comment for why lib/rx/, unlike lib/bootloader/ and
    // lib/usb_cdc/, doesn't need an explicit add_rx.py/lib_ignore bypass.
    rx_start();
    mapping_start();
    control_start();
    output_start();
    servo_start();

#if HELM_HAS_IMU
    // Start the IMU sensor module (issue #14) -- scaffolding only, no
    // real chip decode yet (issue #15 adds that); imu_get_latest()
    // currently always reports SENSOR_STATUS_FAILED. Gated on
    // HELM_HAS_IMU directly, not a HELM_FEATURE_* flag -- onboard sensor
    // presence is a hardware fact, not a deliberate software toggle, per
    // .docs/architecture/sensors.md's "Onboard vs. peripheral" section.
    imu_start();
#endif

#if HELM_HAS_BARO
    // Start the baro sensor module (issue #23) -- same shape as the IMU
    // module above (imu_start()'s own comment): onboard sensor presence
    // is a hardware fact (HELM_HAS_BARO), not a HELM_FEATURE_* toggle.
    baro_start();
#endif

#if HELM_FEATURE_CLI
    // Start the CLI console task (issue #1) -- `status`/`help`/`diag`
    // always, `dfu` too on boards with HELM_HAS_ROM_BOOTLOADER_JUMP set
    // (see cli.c's own gating on that flag).
    cli_start();
#endif

#if HELM_FEATURE_TELEMETRY
    // Start the telemetry gather task (issue #16) -- table + API only so
    // far, nothing real to gather yet (issue #17 adds the first source,
    // #18/#19 add the S.Port/CRSF protocol adapters that actually put it
    // on a wire). See .docs/architecture/telemetry.md.
    telemetry_start();
#endif

#if HELM_FEATURE_TELEMETRY_SPORT && HELM_HAS_SPORT_UART
    // Start the S.Port protocol adapter (issue #18) -- answers this
    // board's own poll slot with TELEM_FIELD_TEST's current value, over
    // UART7/PE8 ("TX7" silk). Independent flags, same reasoning as
    // HELM_FEATURE_CLI/HELM_HAS_ROM_BOOTLOADER_JUMP above: "wants S.Port"
    // and "has a ported S.Port UART transport" aren't the same thing.
    sport_start();
#endif

    // Start the heartbeat task to physically show the board is live. Useful
    // as a bring-up/debug signal independent of the CLI (which not every
    // board has yet).
    debug_heartbeat_start();
    vTaskStartScheduler();

    for (;;) {
    }
}
