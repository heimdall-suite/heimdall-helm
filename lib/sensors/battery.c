#include "battery.h"

#include "board_features.h"

/* Whole-file guard, same idiom lib/telemetry/sport.c already uses for
   HELM_HAS_SPORT_UART -- compiles to an empty translation unit on any
   board without a ported board_battery_adc_*() transport (board.h). */
#if HELM_HAS_BATTERY_SENSE

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"
#include "board.h"

/* Divider/shunt scale constants -- specific to this exact board's PDB
   (matek_h743, the only board with HELM_HAS_BATTERY_SENSE set so far),
   ported verbatim from aoa-boat-controller's config.h, not re-derived:

   - VBAT: 21.0, bench-confirmed 2026-09-10 against a known 9.85V
     battery on this exact hardware (a generic Matek-H743-family divider
     constant of 11.0, used by other variants, read back ~5.1-5.2V --
     exactly the predicted error, confirming 21.0 is this board's real
     value, not a copy-paste guess).
   - CURR: 66.7, cross-confirmed against Mateksys's own manual but
     explicitly NOT bench-verified against a real calibrated load there
     -- treat current readings as less trustworthy than voltage until
     that's done on this project's own bench too.

   If a second board ever gets its own battery-sense circuit, these need
   to become a per-board fact (board.h-exposed, like HELM_SERVO_COUNT),
   not assumed to carry over -- not worth that abstraction for the one
   board that has this today. */
#define H743_VBAT_DIVIDER_SCALE 21.0f
#define H743_BATT_CURR_SCALE 66.7f
#define ADC_VREF_VOLTS 3.3f
#define ADC_MAX_COUNTS 4095.0f /* 12-bit, matches board_battery_adc_init()'s ADC_RESOLUTION_12B */

#define BATTERY_TASK_PERIOD_MS 500 /* slow-changing value, no reason to poll faster
                                       than baro.c's own 1000ms precedent suggests --
                                       a bit faster here since battery level is more
                                       safety-relevant than pressure/temperature */
#define BATTERY_TASK_PRIORITY 1

static QueueHandle_t battery_queue;

static float adc_raw_to_volts(uint16_t raw) {
    return ((float)raw / ADC_MAX_COUNTS) * ADC_VREF_VOLTS;
}

static void battery_task(void *arg) {
    (void)arg;

    BatterySample fallback = {0};
    fallback.status = SENSOR_STATUS_FAILED;
    SupervisorHandle handle = supervisor_register("battery", battery_queue, &fallback, sizeof(fallback),
                                                    pdMS_TO_TICKS(BATTERY_TASK_PERIOD_MS * 3));

    board_battery_adc_init();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(BATTERY_TASK_PERIOD_MS));

        BatterySample sample;
        sample.voltage_v = adc_raw_to_volts(board_battery_adc_read_vbat_raw()) * H743_VBAT_DIVIDER_SCALE;
        sample.current_a = adc_raw_to_volts(board_battery_adc_read_curr_raw()) * H743_BATT_CURR_SCALE;
        sample.status = SENSOR_STATUS_OK;

        xQueueOverwrite(battery_queue, &sample);
        supervisor_kick(handle);
    }
}

void battery_start(void) {
    battery_queue = xQueueCreate(1, sizeof(BatterySample));

    BatterySample initial = {0};
    initial.status = SENSOR_STATUS_FAILED;
    xQueueOverwrite(battery_queue, &initial);

    /* configMINIMAL_STACK_SIZE * 4, not the bare configMINIMAL_STACK_SIZE
       baro_task/imu_task use -- same reasoning lib/cli/cli.c's own
       cli_task already documents for its own *4: board_battery_adc_init()
       runs on this task's stack, not main()'s, and its call chain
       (HAL_RCCEx_PeriphCLKConfig() + HAL_ADC_Init() +
       HAL_ADCEx_Calibration_Start(), each several HAL frames deep) is
       heavier than baro/imu's plain I2C register pokes -- the bare
       configMINIMAL_STACK_SIZE (512B, FreeRTOSConfig.h) silently
       overflowed here, tripping configCHECK_FOR_STACK_OVERFLOW's hook
       (main.c) and halting the whole board with interrupts disabled
       before any task -- including heartbeat -- ever got a single
       scheduler slot. Bench-confirmed: this board showed zero LED
       activity from the commit that added this call onward, root-caused
       by bisecting back to a known-good commit and comparing stack
       depth against cli_task's own already-precedented exception. */
    xTaskCreate(battery_task, "battery", configMINIMAL_STACK_SIZE * 4, NULL, BATTERY_TASK_PRIORITY, NULL);
}

void battery_get_latest(BatterySample *out) {
    xQueuePeek(battery_queue, out, 0);
}

#endif /* HELM_HAS_BATTERY_SENSE */
