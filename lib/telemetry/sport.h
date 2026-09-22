#ifndef HELM_TELEMETRY_SPORT_H
#define HELM_TELEMETRY_SPORT_H

#include <stdint.h>

/* S.Port protocol adapter (issue #18) -- answers this board's own poll
   slot on a real, physically single-wire, receiver-polled bus. Reads
   telemetry.h's table; never decodes/builds anything else. See
   .docs/architecture/telemetry.md's S.Port section for the design, and
   boards/matek_h743/board.h's own comment for the transport this rides
   on (UART7/PE8, "TX7" silk).

   Bidirectional as of issue #42: poll-response (FC -> radio, issue #18)
   plus a push/write path (radio -> FC, Lua's sportTelemetryPush()
   setting one output.c trim param at a time, #39's params-backed
   min/max/subtrim/reverse) -- ported from Betaflight's own real,
   production telemetry/smartport.c receive state machine, not
   aoa-boat-controller's own never-bench-verified SportSensor/
   SportCommand attempt at the same feature (see sport.c's own
   sport_receive_byte() comment for the full citation and reasoning).
   CLI's `param set` (#39) remains a fully-equivalent fallback for
   setting the same values if this path doesn't get bench-verified
   against a real transmitter in time for a given race.

   sport.c's entire body is guarded on HELM_HAS_SPORT_UART -- boards
   without a ported board_sport_uart_* transport (board.h) compile this
   to an empty translation unit, same in-file-guard idiom lib/cli/cli.c
   already uses for its HELM_HAS_ROM_BOOTLOADER_JUMP-gated dfu command,
   rather than needing a separate lib_ignore entry. Call sport_start()
   only when HELM_FEATURE_TELEMETRY_SPORT && HELM_HAS_SPORT_UART are both
   set -- see src/main.c. */
void sport_start(void);

/* Bench diagnostics -- see .docs/cli.md's `diag sport` entry. Lets a
   bench test tell "receiver isn't polling at all" (pollMarkers stays 0)
   apart from "polling, but our ID never matches" (pollMarkers grows,
   pollMatches doesn't) apart from "genuinely working" (both grow
   together) -- same three-way distinction aoa-boat-controller's own
   pollMarkerCount()/pollCount() diagnostics existed to make, for the
   same reason (a raw "no response yet" observation can't tell those
   apart on its own). Only meaningful, and only ever called, when
   HELM_HAS_SPORT_UART is set -- see diag.c's own guard around its call
   site and sport.c's guard around this function's definition. */
void sport_get_counters(uint32_t *pollMarkers, uint32_t *pollMatches);

#endif /* HELM_TELEMETRY_SPORT_H */
