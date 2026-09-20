#include "params_backend.h"

#include <string.h>
#include "stm32f1xx_hal.h"

/* Last 1KB page of this chip's 128KB flash (STM32F103CB, medium-density
   -- confirmed against stm32f1xx_hal_flash_ex.h's own
   FLASH_PAGE_SIZE=0x400 definition for this density class, NOT the
   0x800 high-density value). Page address 0x0801FC00 (0x08000000 +
   0x20000 - 0x400). Deliberately far from firmware code, which starts at
   the opposite end (0x08000000) -- this bring-up stub is nowhere near
   127KB.

   Unlike matek_h743/stm32h7.c, this chip's page (not sector) erase
   granularity is already small enough that aoa-boat-controller never
   needed a Betaflight-style partial-program driver on its own F103
   target (that project's Naze32/UserPinConfig.cpp just used stock
   STM32duino EEPROM.get()/put() directly -- see that file's own
   comment): erasing 1KB and reprogramming a few dozen bytes is cheap on
   this chip, no multi-second full-region rewrite the way H743's 128KB
   sectors caused there. Still routed through the same
   params_backend_read()/write() contract as stm32h7.c, though, so
   params.c stays completely chip-agnostic -- issue #32's whole point. */
static uint32_t const kPageBase = 0x0801FC00U;

/* HAL_FLASH_Program on F1 only supports 16-bit halfword programming (no
   word/doubleword mode the way F4/H7 have -- confirmed against
   stm32f1xx_hal_flash.h's FLASH_TYPEPROGRAM_* list, which has no wider
   option for this family). PARAMS_REGION_SIZE must be even or the last
   program call would run past the buffer. */
_Static_assert(PARAMS_REGION_SIZE % 2U == 0U,
               "PARAMS_REGION_SIZE must be a multiple of F1's 2-byte halfword program size");

void params_backend_read(uint32_t offset, void *dest, uint32_t len) {
    /* Flash is directly memory-mapped and readable -- no HAL call
       needed, same as stm32h7.c's own read path. */
    memcpy(dest, (void const *)(kPageBase + offset), len);
}

bool params_backend_write(uint32_t offset, void const *src, uint32_t len) {
    /* Read-modify-write against the small logical region, not blindly
       trusting the rest of the page -- same shape stm32h7.c uses,
       scaled to this chip's own (much smaller) physical erase unit. */
    uint8_t buffer[PARAMS_REGION_SIZE];
    params_backend_read(0, buffer, PARAMS_REGION_SIZE);
    memcpy(buffer + offset, src, len);

    HAL_FLASH_Unlock();
    /* F1 (this specific part, non-dual-bank) has no combined "all
       errors" flag macro the way H7's FLASH_FLAG_ALL_ERRORS_BANK2 is --
       confirmed by reading stm32f1xx_hal_flash_ex.h directly, same
       "verify against the real header, don't assume the other chip's
       shape carries over" standard as stm32h7.c's own comment on this.
       Clear both individual error flags this chip actually has. */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    FLASH_EraseInitTypeDef eraseInit = {0};
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = kPageBase;
    eraseInit.NbPages = 1;
    uint32_t pageError = 0;

    /* HAL_FLASHEx_Erase() sets pageError to 0xFFFFFFFF on full success,
       or the address of the first page that failed otherwise (confirmed
       against stm32f1xx_hal_flash_ex.c) -- same convention as H7's
       sectorError, and the same check stm32h7.c's own comment explains
       aoa-boat-controller's ConfigFlash skipped entirely
       (params_backend.h's flash-safety requirement). */
    bool ok = (HAL_FLASHEx_Erase(&eraseInit, &pageError) == HAL_OK) && (pageError == 0xFFFFFFFFU);

    for (uint32_t i = 0; ok && i < PARAMS_REGION_SIZE; i += 2U) {
        uint16_t halfword;
        /* memcpy, not a cast-and-dereference -- buffer + i isn't
           guaranteed 2-byte aligned for every i, and this core does not
           need an unaligned-access fault to prove that assumption
           wrong. */
        memcpy(&halfword, buffer + i, sizeof(halfword));
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, kPageBase + i, halfword) == HAL_OK;
    }

    HAL_FLASH_Lock();

    return ok;
}
