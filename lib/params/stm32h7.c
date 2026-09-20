#include "params_backend.h"

#include <string.h>
#include "stm32h7xx_hal.h"

/* Last 128KB sector of this chip's 2MB flash (STM32H743VIT6, the same
   physical part as matek_h743, confirmed via platformio.ini's
   devebox_h743vitx board def) -- Bank 2, Sector 7, address 0x081E0000
   (0x08000000 + 15 * 0x20000). Deliberately far from firmware code,
   which starts at the opposite end (0x08000000). Same sector
   aoa-boat-controller's own lib/Config/H743/ConfigFlash.cpp reserves on
   its identical physical board, ported here with one real fix: every HAL
   erase/program call's return status is now checked
   (params_backend_write()'s own comment) -- that file's writeBytes()
   never did, the suspected cause of a real "booted into DFU fine, failed
   to erase while flashing" bug there. */
static uint32_t const kSectorBase = 0x081E0000U;
static uint32_t const kBank = FLASH_BANK_2;
static uint32_t const kSector = FLASH_SECTOR_7;

/* HAL_FLASH_Program's FLASHWORD mode on H7 writes 32 bytes (256 bits) at
   a time -- confirmed against stm32h7xx_hal_flash.h's own
   FLASH_TYPEPROGRAM_FLASHWORD comment, same constraint
   aoa-boat-controller's ConfigFlash.cpp documents. PARAMS_REGION_SIZE
   must be a whole multiple of it or the last program call would run
   past the buffer. */
_Static_assert(PARAMS_REGION_SIZE % 32U == 0U,
               "PARAMS_REGION_SIZE must be a multiple of H7's 32-byte FLASHWORD program size");

void params_backend_read(uint32_t offset, void *dest, uint32_t len) {
    /* Flash is directly memory-mapped and readable -- no HAL call
       needed, same as ConfigFlash::readBytes(). */
    memcpy(dest, (void const *)(kSectorBase + offset), len);
}

bool params_backend_write(uint32_t offset, void const *src, uint32_t len) {
    /* Read-modify-write against the small logical region, not the whole
       physical sector -- preserves every other byte already in it
       (there's only one param today, but this store is meant to grow),
       same shape ConfigFlash::writeBytes() uses on this same chip. */
    uint8_t buffer[PARAMS_REGION_SIZE];
    params_backend_read(0, buffer, PARAMS_REGION_SIZE);
    memcpy(buffer + offset, src, len);

    HAL_FLASH_Unlock();
    /* Per-bank error flags on H7 (dual-bank flash) -- there's no single
       combined "all errors" macro the way single-bank STM32 parts have
       (confirmed by reading stm32h7xx_hal_flash.h directly). Bank 2
       specifically, since kSectorBase/kBank point there. */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK2);

    FLASH_EraseInitTypeDef eraseInit = {0};
    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.Banks = kBank;
    eraseInit.Sector = kSector;
    eraseInit.NbSectors = 1;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    uint32_t sectorError = 0;

    /* HAL_FLASHEx_Erase() sets sectorError to 0xFFFFFFFF on full
       success, or the index of the first sector that failed otherwise
       (confirmed against stm32h7xx_hal_flash_ex.c) -- both the return
       status AND this output must be checked; a HAL_OK return alone
       doesn't guarantee every requested sector actually erased. This is
       exactly the check aoa-boat-controller's ConfigFlash::writeBytes()
       skipped (params_backend.h's own comment). */
    bool ok = (HAL_FLASHEx_Erase(&eraseInit, &sectorError) == HAL_OK) && (sectorError == 0xFFFFFFFFU);

    for (uint32_t i = 0; ok && i < PARAMS_REGION_SIZE; i += 32U) {
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, kSectorBase + i, (uint32_t)(buffer + i)) == HAL_OK;
    }

    HAL_FLASH_Lock();

    return ok;
}
