#include "bootloader.h"
#include "stm32h7xx_hal.h"
#include <stdbool.h>

/* Software-triggered jump into the STM32H743's built-in ROM DFU
   bootloader (USB DFU, VID:PID 0483:df11). Ported from this project's
   sibling `aoa-boat-controller` (lib/Diagnostics/H743/RomBootloader),
   which itself ported Betaflight's real, deployed
   src/platform/STM32/system_stm32h7xx.c (systemResetToBootloader()/
   systemJumpToBootloader()/systemProcessResetReason()) and
   src/platform/STM32/persistent.c (RTC backup-register persistence) --
   confirmed working on this exact physical board (dfu-util enumerated a
   real 0483:df11 device after a plain `bl rom` over the USB CLI, no
   button press, see that project's docs/decisions.md). Trimmed to plain C
   and this project's own HAL calls; the RTC-backup-register technique,
   magic value, and bootloader vector table address are otherwise
   unchanged from that bench-confirmed source. */

/* RTC backup register used for this project's own bootloader-request
   flag. Arbitrary but documented, same reasoning as the ported source:
   nothing else here uses RTC backup registers yet. */
#define BOOT_REQUEST_BKP_REGISTER RTC_BKP_DR0

/* Arbitrary 32-bit sentinel, chosen to be unlikely to appear from a
   genuinely uninitialized/garbage backup register (e.g. stale contents
   from this same unit's previous life running different firmware). */
#define BOOT_REQUEST_MAGIC 0xB007D0FEU

/* STM32H743/750/723/725/730/735/757's system-memory bootloader vector
   table address -- confirmed against Betaflight's real, deployed
   system_stm32h7xx.c (SYSMEMBOOT_VECTOR_TABLE) and cross-checked against
   ST community threads, NOT the generic 0x1FF00000 "system memory base"
   AN2606 lists for the region as a whole. ST's ROM bootloader expects the
   jump to land on the vector table at this specific offset for H74x/H75x
   parts. */
static uint32_t *const kSysMemBootVectorTable = (uint32_t *)0x1FF09800U;

typedef void (*BootJumpFn)(void);

/* Unlock backup-domain writes and enable the RTC peripheral so the backup
   registers can be read/written. H7 doesn't gate the PWR peripheral clock
   the way F4/F7 do, so this is just the enable-access + write-protection
   toggle sequence. */
static void enable_backup_register_access(void) {
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_RTC_ENABLE();

    RTC_HandleTypeDef rtcHandle = {0};
    rtcHandle.Instance = RTC;
    __HAL_RTC_WRITEPROTECTION_ENABLE(&rtcHandle);
    __HAL_RTC_WRITEPROTECTION_DISABLE(&rtcHandle);
}

/* Set the main stack pointer to the bootloader's own initial value, then
   jump to its reset vector. Never returns. Called before HAL_Init()/
   board_init() run, so clocks/caches/interrupts are all still at their
   power-on-reset defaults -- nothing needs undoing first. */
static void jump_to_system_bootloader(void) {
    __HAL_RCC_SYSCFG_CLK_ENABLE();

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
       contents without this actually being a bootloader request. */
    bool const wasSoftReset = (RCC->RSR & RCC_RSR_SFTRSTF) != 0;

    enable_backup_register_access();

    RTC_HandleTypeDef rtcHandle = {0};
    rtcHandle.Instance = RTC;
    uint32_t const backupValue = HAL_RTCEx_BKUPRead(&rtcHandle, BOOT_REQUEST_BKP_REGISTER);

    if (wasSoftReset && backupValue == BOOT_REQUEST_MAGIC) {
        /* Consume the request so a subsequent normal reset (e.g. the user
           power-cycling after DFU-flashing) doesn't loop back into the
           bootloader again. */
        HAL_RTCEx_BKUPWrite(&rtcHandle, BOOT_REQUEST_BKP_REGISTER, 0);
        jump_to_system_bootloader();
    }
}

void bootloader_request_dfu(void) {
    enable_backup_register_access();

    RTC_HandleTypeDef rtcHandle = {0};
    rtcHandle.Instance = RTC;
    HAL_RTCEx_BKUPWrite(&rtcHandle, BOOT_REQUEST_BKP_REGISTER, BOOT_REQUEST_MAGIC);

    __disable_irq();
    NVIC_SystemReset();
}
