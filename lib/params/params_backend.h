#ifndef HELM_PARAMS_BACKEND_H
#define HELM_PARAMS_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

/* Internal contract between params.c's chip-agnostic registry/cache/CRC
   logic and whichever chip file a board's env explicitly selects via
   custom_helm_params_backend -- stm32h7.c (matek_h743) or stm32f1.c
   (afroflight32). Not part of params.h's public API: only params.c and
   these chip files need to agree on it. Mirrors lib/sensors/imu_chip.h's
   split (board-agnostic plumbing vs. chip-specific primitive), except the
   primitive here is a reserved flash region, not a sensor bus
   transaction. */

/* Logical size of the region params.c reads/writes as one unit --
   deliberately small and identical on both chips (params.c's own record
   must fit inside it, checked at compile time there). Each backend's
   REAL, physical erase unit is much bigger and differs per chip (a
   128KB sector on the H743, a 1KB page on the F103) -- same "erase
   whole physical unit once, reprogram only this small logical window"
   split aoa-boat-controller's ConfigFlash uses on H743 (that project's
   kRegionSize), scaled down here since this store currently holds one
   test param, not that project's five real config records. */
#define PARAMS_REGION_SIZE 64U

/* Reads len bytes starting at offset (0-based into the logical region
   above, not an absolute flash address) into dest. Flash is directly
   memory-mapped on both chip families this project targets, so this can
   never itself fail once offset/len are in range -- callers (params.c)
   own that bounds check, same convention as aoa-boat-controller's
   ConfigFlash::readBytes(). */
void params_backend_read(uint32_t offset, void *dest, uint32_t len);

/* Erases the whole reserved physical region (unavoidably page/sector-
   granular -- see each chip file's own comment for its actual erase
   unit) and reprograms PARAMS_REGION_SIZE bytes starting from a local
   read-modify-write buffer with src patched in at offset, preserving
   every other byte in the region.

   Returns false, and leaves flash in whatever state the failing HAL call
   left it, if EITHER the erase or any individual program call reports
   failure -- callers (params.c) must treat false as "this value may not
   actually be persisted, do not trust it survived a reboot," never as a
   soft warning to ignore. This is issue #32's flash-safety requirement:
   the prior art this module was ported from (aoa-boat-controller's
   ConfigFlash::writeBytes()) never checked its own HAL_FLASHEx_Erase()/
   HAL_FLASH_Program() return codes and proceeded regardless -- suspected
   root cause of a real "booted into DFU fine, failed to erase while
   flashing" bug there (flash left in a bad state by an earlier silent
   failure). Every backend implementation must check every one of these
   calls. */
bool params_backend_write(uint32_t offset, void const *src, uint32_t len);

#endif /* HELM_PARAMS_BACKEND_H */
