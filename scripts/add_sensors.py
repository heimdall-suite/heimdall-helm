Import("env")

# Selects exactly one chip implementation per subsystem per board:
#   IMU  -- icm42688p.c (matek_h743, #26), mpu6500.c (afroflight32, #27),
#           imu_stub.c (any board without a real driver yet)
#   Baro -- dps310.c (matek_h743, #23), bmp280.c (afroflight32, #23),
#           baro_stub.c (any board without a real driver yet)
# battery.c (issue #24) isn't a per-chip selection -- there's no chip,
# just a raw ADC pin, so it's a single file, self-guarded on
# HELM_HAS_BATTERY_SENSE (same whole-file-guard idiom lib/telemetry/
# sport.c uses for HELM_HAS_SPORT_UART), compiled unconditionally here
# alongside imu.c/baro.c below rather than needing its own
# custom_helm_battery selection.
# Same explicit-source-selection reasoning as add_bootloader.py/
# add_usb_cdc.py, since lib/sensors/ has more than one file implementing
# each of imu_chip.h's/baro_chip.h's same function names. lib_ignore =
# sensors is set in every env in platformio.ini; every env adds this
# script to their extra_scripts, since every board has at least the
# *_stub.c fallback for each subsystem.
#
# imu.c/baro.c themselves are NOT chip-specific choices -- they're the
# board/chip-agnostic task/queue plumbing (issue #14, #23) that calls
# whichever chip file custom_helm_imu/custom_helm_baro names, so they're
# compiled unconditionally here alongside those two selections, same as
# sensors.h/imu.h/imu_chip.h/baro.h/baro_chip.h being found via this
# script's own CPPPATH addition rather than PlatformIO's default lib/
# include scan.
project_dir = env.subst("$PROJECT_DIR")
sensors_dir = project_dir + "/lib/sensors"

# Absent entirely (rather than every board naming "stub"/"baro_stub"
# explicitly) is read the same way custom_helm_stm32cube_package's
# absence is in add_usb_cdc.py: "no real driver for this board/subsystem
# yet," not a missing option.
imu_chip = env.GetProjectOption("custom_helm_imu", "imu_stub")
baro_chip = env.GetProjectOption("custom_helm_baro", "baro_stub")

env.Append(CPPPATH=[sensors_dir])

# imu.c/baro.c genuinely depend on lib/supervisor/ (each registers with
# the module-liveness supervisor, same as every other module's task) --
# when they were part of PlatformIO's normal LDF auto-scan, LDF's own
# chain-following added supervisor's directory to CPPPATH automatically
# by parsing their #include lines itself. Now that lib/sensors/ is
# explicit-selected (lib_ignore'd out of that scan, per this file's
# header comment), that resolution has to happen here instead -- LDF
# still discovers lib/supervisor on its own (main.c includes
# "supervisor.h" directly too), but only *after* every pre: extra_script
# (this one included) has already run, too late for this compile.
env.Append(CPPPATH=[project_dir + "/lib/supervisor"])

env.BuildSources(
    "$BUILD_DIR/Sensors",
    sensors_dir,
    src_filter=[
        "-<*>",
        "+<imu.c>",
        "+<" + imu_chip + ".c>",
        "+<baro.c>",
        "+<" + baro_chip + ".c>",
        "+<battery.c>",
    ],
)
