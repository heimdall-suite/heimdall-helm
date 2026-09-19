#include "bootloader.h"
#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* Software-triggered jump into the STM32F103's built-in ROM UART
   bootloader (AN3155 protocol, 8E1 framing -- confirmed by this
   project's own `stm32flash ... 115200 8E1` upload output) -- reached
   over the SAME USART1 this board's own CLI already runs over via its
   onboard USB-serial converter chip (see lib/usb_cdc/stm32f1.c's own
   header comment), not USB DFU class the way matek_h743's is. Issue #28.

   The jump address and two-stage "persist a flag, reset, check the flag
   before anything else runs" shape are ported from Cleanflight's real,
   historically deployed src/main/drivers/system_stm32f10x.c
   (systemResetToBootloader()/checkForBootLoaderRequest()), which flew
   this exact technique on this exact chip variant -- STM32F103, 20KB
   RAM, confirmed by that file's own "20KB STM32F103" comment, the same
   density class as this board's F103CB. Confirms the jump address itself
   (0x1FFFF000 -- AN2606's low/medium-density STM32F10x system-memory
   base; re-verify against that document directly before trusting this
   comment alone) and, more importantly, that a raw same-session jump
   isn't safe here: unlike a genuine reset, it wouldn't undo whatever
   clock/peripheral state this board's own firmware left behind, which
   the ROM bootloader doesn't expect to find already configured.

   Where this driver deliberately differs from Cleanflight's: Cleanflight
   persists its flag in a fixed, guessed SRAM address (0x20004FF0, 16
   bytes below this exact chip variant's top-of-RAM) that survives a soft
   reset by convention, not a documented hardware guarantee -- safe
   enough for Cleanflight's own bare-metal build, but fragile against
   this project's own linker layout and FreeRTOS heap/stack placement,
   which Cleanflight never had to share that memory with. This driver
   uses the chip's real backup registers instead (the BKP peripheral,
   RTC_BKP_DR1..DR10 via HAL_RTCEx_BKUPWrite/Read -- F1 has no DR0,
   unlike H7) -- a real, VBAT-backed, soft-reset-surviving hardware
   mechanism this chip actually has for exactly this purpose, same
   pattern this project's own stm32h7.c already uses on a different chip
   family for the identical reason, just this family's own access
   sequence (separate PWR + BKP peripheral clocks, HAL_PWR_EnableBkUpAccess()
   -- no RTC peripheral itself needs enabling, unlike H7's approach). */

/* F1's backup registers are 1-indexed (no DR1 is the first, unlike H7's
   DR0) and 16-bit wide (BKP_DR1_D is a 16-bit field) -- both confirmed
   against this project's own copy of stm32f1xx_hal_rtc_ex.h. */
#define BOOT_REQUEST_BKP_REGISTER RTC_BKP_DR1
#define BOOT_REQUEST_MAGIC 0xB007U

/* STM32F103 (low/medium-density)'s system-memory bootloader base --
   MSP at +0, reset vector at +4, same vector-table layout every
   Cortex-M ROM/flash region uses, just at this fixed address instead of
   the usual flash-relative one. See this file's own header comment for
   the Cleanflight source this was confirmed against. */
static uint32_t *const kSysMemBootVectorTable = (uint32_t *)0x1FFFF000U;

typedef void (*BootJumpFn)(void);

/* Enable backup-domain write access -- F1's own sequence (separate PWR
   and BKP peripheral clocks, then the same DBP-bit toggle H7 uses) --
   distinct from H7's enable_backup_register_access(), which also has to
   enable the RTC peripheral itself; F1's backup registers don't need
   that. */
static void enable_backup_register_access(void) {
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
}

/* Set the main stack pointer to the bootloader's own initial value, then
   jump to its reset vector. Never returns. Called before HAL_Init()/
   board_init() run, so clocks/interrupts are all still at their
   power-on-reset defaults -- nothing needs undoing first. */
static void jump_to_system_bootloader(void) {
    uint32_t const bootStack = kSysMemBootVectorTable[0];
    BootJumpFn const sysMemBootJump = (BootJumpFn)kSysMemBootVectorTable[1];

    __set_MSP(bootStack);
    sysMemBootJump();

    for (;;) {
        /* unreachable -- sysMemBootJump() never returns */
    }
}

void bootloader_jump_if_requested(void) {
    /* Only trust the backup register's value if this was actually a
       software reset -- a real power-cycle can leave stale backup-domain
       contents without this actually being a bootloader request, same
       reasoning (and same RCC->CSR-direct style) stm32h7.c's own check
       uses. */
    bool const wasSoftReset = (RCC->CSR & RCC_CSR_SFTRSTF) != 0;

    enable_backup_register_access();

    RTC_HandleTypeDef rtcHandle = {0};
    uint32_t const backupValue = HAL_RTCEx_BKUPRead(&rtcHandle, BOOT_REQUEST_BKP_REGISTER);

    if (wasSoftReset && backupValue == BOOT_REQUEST_MAGIC) {
        /* Consume the request so a subsequent normal reset (e.g. the
           user power-cycling after reflashing) doesn't loop back into
           the bootloader again. */
        HAL_RTCEx_BKUPWrite(&rtcHandle, BOOT_REQUEST_BKP_REGISTER, 0);
        jump_to_system_bootloader();
    }
}

void bootloader_request_dfu(void) {
    enable_backup_register_access();

    RTC_HandleTypeDef rtcHandle = {0};
    HAL_RTCEx_BKUPWrite(&rtcHandle, BOOT_REQUEST_BKP_REGISTER, BOOT_REQUEST_MAGIC);

    __disable_irq();
    NVIC_SystemReset();
}
