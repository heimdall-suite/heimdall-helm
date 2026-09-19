#include "usb_cdc.h"
#include "stm32f1xx_hal.h"

/* CLI transport for afroflight32 -- NOT a native USB CDC device. This
   chip's own USB peripheral is never touched here at all.

   Confirmed against aoa-boat-controller's real, currently-running
   firmware on this exact physical board (src/Naze32/main.cpp,
   include/pins_naze32.h): its "USB" port is an onboard USB-serial
   converter chip wired to USART1 (PA9 TX / PA10 RX), doing the
   USB<->UART conversion entirely in external hardware, invisible to
   this MCU -- confirmed explicitly in that source: "No native USB CDC
   on this board (Serial1 is a plain UART through an external
   USB-serial converter chip...)". PA11/PA12 (this chip's native USB
   D-/D+ pins) aren't wired to that connector at all -- PA11 is actually
   this board's OUT2 servo-PWM pad on the real hardware.

   An earlier version of this file wrongly assumed a native USB Device
   peripheral, mirroring matek_h743/lib/usb_cdc/stm32h7.c's shape,
   before aoa-boat-controller's real source was checked -- see issue
   #12's history for that correction. Kept the same usb_cdc.h interface
   deliberately: from lib/cli's perspective (and the host's), this still
   presents as a normal serial console over what the user plugs a USB
   cable into -- which transport achieves that is exactly what this
   header exists to hide.

   115200 baud, 8N1: aoa-boat-controller's own confirmed working value
   for this exact UART/converter pairing (Serial1.begin(115200) in that
   project's main.cpp), not picked fresh here.

   Bench-verified on the real unit (#13): `status`/`help`/`diag pipeline`
   all round-trip correctly. Getting there also needed two fixes outside
   this file -- src/main.c's SCB->VTOR correction (this board's
   bootloader-jump entry otherwise leaves interrupts, including SysTick,
   vectoring into the ROM bootloader's own stale table) and a
   configTOTAL_HEAP_SIZE increase in FreeRTOSConfig.h -- see both files'
   own comments. */

#define RX_RING_SIZE 256U

static UART_HandleTypeDef huart1;
static uint8_t rxByte;

static uint8_t rxRing[RX_RING_SIZE];
static volatile uint32_t rxHead = 0; /* next slot the ISR writes */
static volatile uint32_t rxTail = 0; /* next slot the reader takes */

/* HAL callback (weak override): fires once per received byte. Byte-at-a-
   time interrupt receive, not DMA -- this is a low-rate (115200 baud,
   ~11.5KB/s) interactive CLI stream of unknown/unbounded length, not a
   fixed-size framed protocol like SBUS, so there's no natural DMA
   transfer size to arm ahead of time the way board_sbus_uart_rearm()
   has for matek_h743. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART1) {
        return;
    }

    /* Drop bytes on ring overflow rather than block -- a CLI console
       losing a byte under overflow is fine, an ISR blocking is not,
       same reasoning as stm32h7.c's own USB CDC ring buffer. */
    uint32_t const next = (rxHead + 1U) % RX_RING_SIZE;
    if (next != rxTail) {
        rxRing[rxHead] = rxByte;
        rxHead = next;
    }

    HAL_UART_Receive_IT(&huart1, &rxByte, 1);
}

/* HAL callback (weak override): fires when HAL_UART_IRQHandler sees a
   parity/framing/noise/overrun error, which HAL treats as fatal to the
   in-flight interrupt-driven reception -- it aborts the
   HAL_UART_Receive_IT() transfer entirely before calling this. Real
   risk here, not hypothetical: the STM32 ROM serial bootloader uses this
   exact USART1 peripheral to receive the firmware image over the same
   wire moments before jumping to this app (that's how `pio run -t
   upload` with upload_protocol = serial gets code onto this board at
   all -- see platformio.ini's own comment on that). Without a full
   hardware reset in between (BOOT0 unstrapped alone doesn't force one,
   only an actual reset/power-cycle re-samples it), a leftover framing/
   overrun condition on this peripheral is a real way for the very first
   HAL_UART_Receive_IT arm in usb_cdc_init() to immediately abort with no
   further symptom -- no crash, no LED (this board has none,
   HELM_HAS_DEBUG_LED 0), just permanently dead RX despite TX and
   everything else working fine, which is exactly indistinguishable from
   "board never booted" without a debug probe. HAL's own default weak
   HAL_UART_ErrorCallback does nothing, so without this override that one
   error is unrecoverable. Same bug class (and same fix -- just re-arm)
   as matek_h743/board.c's own HAL_UART_ErrorCallback for its SBUS UART,
   found there for an unrelated reason (line noise) but the underlying
   HAL behavior is identical. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART1) {
        return;
    }

    HAL_UART_Receive_IT(&huart1, &rxByte, 1);
}

/* HAL callback (weak override): GPIO/clock/NVIC for USART1 -- PA9/PA10
   is this chip's default (non-remapped) USART1 mapping on every F103
   package, a fixed silicon fact, not a board-specific pin choice. */
void HAL_UART_MspInit(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART1) {
        return;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = GPIO_PIN_9;
    gpioInit.Mode = GPIO_MODE_AF_PP;
    gpioInit.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpioInit);

    gpioInit.Pin = GPIO_PIN_10;
    gpioInit.Mode = GPIO_MODE_INPUT;
    gpioInit.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpioInit);

    /* Priority 5: this project's established floor for any ISR that runs
       alongside FreeRTOS -- same reasoning as every other peripheral ISR
       in this repo (see e.g. matek_h743/board.c's UART IRQ comment).
       HAL_UART_RxCpltCallback above only touches a plain ring buffer, no
       FreeRTOS API today, but keeping every peripheral ISR at or below
       this floor by default avoids that becoming a live bug later. */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* Weak override of the CMSIS startup file's default handler
   (startup_stm32f103xb.s : USART1_IRQHandler -> Default_Handler). */
void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}

void usb_cdc_init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }

    HAL_UART_Receive_IT(&huart1, &rxByte, 1);
}

uint32_t usb_cdc_read(uint8_t *buf, uint32_t maxLen) {
    uint32_t count = 0;
    while (count < maxLen && rxTail != rxHead) {
        buf[count] = rxRing[rxTail];
        rxTail = (rxTail + 1U) % RX_RING_SIZE;
        count++;
    }
    return count;
}

void usb_cdc_write(const uint8_t *buf, uint32_t len) {
    /* Blocking, generous timeout -- acceptable for a debug console that
       only ever sends short lines (SHELL_MAX_LINE_LEN, lib/shell/
       shell.h, is 64), not for a link this project depends on for
       control-relevant data. Cast to uint16_t is safe for the same
       reason -- len never approaches 65535 for a line-based CLI. */
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 1000);
}
