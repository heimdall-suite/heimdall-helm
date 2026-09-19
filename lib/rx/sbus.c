#include "rx.h"
#include "shared/rx_timeout.h"

#include "board_features.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>

#if HELM_HAS_SBUS_UART
#include "board.h"
#endif

/* SBUS driver: real UART/DMA decode (issue #8) on boards with a confirmed
   SBUS UART wiring (HELM_HAS_SBUS_UART, board_features.h), fed from
   board_sbus_uart_take_frame() (board.c owns the actual UART/pin/DMA --
   see that header's own comment for the pin/inverter/DMA provenance on
   matek_h743, the only board with one so far). This file owns everything
   protocol-level: frame validation, the bit-unpacking math, the explicit
   frame-lost/failsafe bits, and layering RxTimeoutWatchdog underneath
   them as a backstop -- see .docs/architecture/receiver-to-servo.md's
   Failsafe section. Boards without a confirmed wiring yet (afroflight32,
   nexus_xr) fall back to the fixed-test-data stub every driver used to be
   before this issue, unchanged.

   Byte-level layout and the 11-bit channel unpacking are a direct port of
   this project's sibling aoa-boat-controller's lib/Sbus/H743/
   SbusReceiver.cpp (bench-confirmed working on this same physical
   receiver wiring), which itself mirrors bolderflight/sbus
   (github.com/bolderflight/sbus) -- fiddly bit-packing with a proven
   reference, not re-derived here. One deliberate difference: that
   version's byte-by-byte resync state machine (looking for a footer-then-
   header pair to realign after any dropped byte) isn't ported, because it
   solves a problem this driver doesn't have -- board_sbus_uart_take_frame()
   already only ever hands over a capture that starts right after a real
   idle-line gap, so a short/misaligned capture is simply discarded
   (header/footer check below) and the next one is already realigned, no
   persistent resync state needed. "Fast SBUS" (alternate footer byte/baud
   variant) isn't handled, same as that reference. */

#if HELM_HAS_SBUS_UART

#define SBUS_HEADER_BYTE 0x0FU
#define SBUS_FOOTER_BYTE 0x00U

/* Bits within the 24th (flags) byte -- ch17/ch18 (bits 0/1) aren't part of
   RxFrame's 16-channel contract (rx.h) and are intentionally dropped. */
#define SBUS_FLAG_BIT_FRAME_LOST 0x04U
#define SBUS_FLAG_BIT_FAILSAFE 0x08U

/* SBUS's own nominal frame period is 14ms (this driver doesn't handle
   "Fast SBUS" 7ms mode, see above) -- 3x that, same "a few missed periods,
   not one" margin rx.c's own supervisor timeout already uses for its
   placeholder period, before the backstop watchdog (not the frame's own
   failsafe bit) declares the link dead. */
#define SBUS_FRAME_PERIOD_MS 14
#define SBUS_TIMEOUT_MS (SBUS_FRAME_PERIOD_MS * 3)

static RxTimeoutWatchdog timeout_watchdog;
static RxFrame latest;

/* b[0]=header, b[1..22]=16 channels x 11 bits packed LSB-first, b[23]=flags,
   b[24]=footer -- ported unchanged from aoa-boat-controller's decode(). */
static void sbus_decode(const uint8_t *b) {
    latest.channels[0] = (uint16_t)(b[1] | ((b[2] << 8) & 0x07FF));
    latest.channels[1] = (uint16_t)((b[2] >> 3) | ((b[3] << 5) & 0x07FF));
    latest.channels[2] = (uint16_t)((b[3] >> 6) | (b[4] << 2) | ((b[5] << 10) & 0x07FF));
    latest.channels[3] = (uint16_t)((b[5] >> 1) | ((b[6] << 7) & 0x07FF));
    latest.channels[4] = (uint16_t)((b[6] >> 4) | ((b[7] << 4) & 0x07FF));
    latest.channels[5] = (uint16_t)((b[7] >> 7) | (b[8] << 1) | ((b[9] << 9) & 0x07FF));
    latest.channels[6] = (uint16_t)((b[9] >> 2) | ((b[10] << 6) & 0x07FF));
    latest.channels[7] = (uint16_t)((b[10] >> 5) | ((b[11] << 3) & 0x07FF));
    latest.channels[8] = (uint16_t)(b[12] | ((b[13] << 8) & 0x07FF));
    latest.channels[9] = (uint16_t)((b[13] >> 3) | ((b[14] << 5) & 0x07FF));
    latest.channels[10] = (uint16_t)((b[14] >> 6) | (b[15] << 2) | ((b[16] << 10) & 0x07FF));
    latest.channels[11] = (uint16_t)((b[16] >> 1) | ((b[17] << 7) & 0x07FF));
    latest.channels[12] = (uint16_t)((b[17] >> 4) | ((b[18] << 4) & 0x07FF));
    latest.channels[13] = (uint16_t)((b[18] >> 7) | (b[19] << 1) | ((b[20] << 9) & 0x07FF));
    latest.channels[14] = (uint16_t)((b[20] >> 2) | ((b[21] << 6) & 0x07FF));
    latest.channels[15] = (uint16_t)((b[21] >> 5) | ((b[22] << 3) & 0x07FF));
}

static void sbus_init(void) {
    /* No frame has ever arrived yet -- default to FAILSAFE (RX_STATUS_OK
       is enum value 0, so this must be set explicitly rather than relying
       on latest's static zero-init). rx_timeout_init() below still seeds
       a startup grace period (SBUS_TIMEOUT_MS to receive a first real
       frame) rather than the backstop tripping instantly at boot. */
    latest = (RxFrame){0};
    latest.status = RX_STATUS_FAILSAFE;

    board_sbus_uart_init();
    rx_timeout_init(&timeout_watchdog, SBUS_TIMEOUT_MS);
}

static void sbus_poll(RxFrame *out) {
    uint8_t raw[SBUS_UART_FRAME_LEN];

    if (board_sbus_uart_take_frame(raw) && raw[0] == SBUS_HEADER_BYTE &&
        raw[SBUS_UART_FRAME_LEN - 1] == SBUS_FOOTER_BYTE) {
        sbus_decode(raw);

        /* Any well-formed frame -- failsafe bit or not -- proves the
           receiver-to-FC link itself is alive, so it resets the backstop
           regardless of what the fast-path status ends up being below. A
           cut wire or crashed receiver is the only way no more well-
           formed frames arrive at all, bit or no bit. */
        rx_timeout_note_valid_frame(&timeout_watchdog);

        bool const frameLost = (raw[23] & SBUS_FLAG_BIT_FRAME_LOST) != 0;
        bool const failsafe = (raw[23] & SBUS_FLAG_BIT_FAILSAFE) != 0;

        if (frameLost) {
            latest.frame_loss_count++;
        }
        latest.status = failsafe ? RX_STATUS_FAILSAFE : RX_STATUS_OK;
    }
    /* No new capture, or an invalid/misaligned one: channels and status
       are left exactly as they were -- "hold last known value" is just
       what not writing anything already looks like, per
       .docs/architecture/receiver-to-servo.md's Failsafe section. */

    /* Backstop: overrides the fast path above if nothing well-formed has
       arrived for SBUS_TIMEOUT_MS, regardless of what the last frame's
       own bit said. */
    if (rx_timeout_has_expired(&timeout_watchdog)) {
        latest.status = RX_STATUS_FAILSAFE;
    }

    /* Diagnostic only, per rx.h's own contract on these two fields. */
    latest.last_frame_age_ms =
        (xTaskGetTickCount() - timeout_watchdog.last_valid_frame_tick) * portTICK_PERIOD_MS;

    *out = latest;
}

#else /* !HELM_HAS_SBUS_UART */

/* No confirmed SBUS UART wiring on this board yet -- see
   board_features.h's own comment on HELM_HAS_SBUS_UART. Same fixed-test-
   data stub every RX driver was before issue #8, so the #7 pipeline stays
   bench-verifiable here unchanged. */
static void sbus_init(void) {
}

static void sbus_poll(RxFrame *out) {
    *out = (RxFrame){0};
    out->status = RX_STATUS_OK;
}

#endif /* HELM_HAS_SBUS_UART */

static const rx_driver_t driver = {
    .init = sbus_init,
    .poll = sbus_poll,
};

const rx_driver_t *rx_sbus_driver(void) {
    return &driver;
}
