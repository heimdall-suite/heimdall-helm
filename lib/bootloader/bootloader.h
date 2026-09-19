#ifndef HELM_BOOTLOADER_H
#define HELM_BOOTLOADER_H

/* Software-triggered jump into a chip's built-in ROM DFU bootloader --
   lets the CLI's `bl`/`dfu` command (see lib/cli) reboot straight into
   flash-programming mode over the same USB cable already used for the
   runtime console, instead of needing the physical BOOT button held
   across a power cycle. One implementation per chip that supports it
   (currently just stm32h7.c) -- see lib/README.md's chip-not-board
   convention and platformio.ini's custom_helm_bootloader/
   scripts/add_bootloader.py for how a board opts in. Boards without an
   implementation don't compile this at all -- gate any call to these
   functions behind board_features.h's HELM_HAS_ROM_BOOTLOADER_JUMP. */

/* Call as the literal first statement in main() -- before HAL_Init() or
   board_init() touch any clock/peripheral. If a previous boot called
   bootloader_request_dfu(), this jumps straight into the ROM bootloader
   and never returns. Otherwise it returns immediately and normal boot
   continues. Safe to call unconditionally on every boot (power-on reset
   included) -- implementations must not act on stale/garbage persisted
   state left over from a genuine power cycle. */
void bootloader_jump_if_requested(void);

/* Persists a "boot into DFU" request, then resets. bootloader_jump_if_requested()
   picks it up on the next boot. Never returns. */
void bootloader_request_dfu(void);

#endif /* HELM_BOOTLOADER_H */
