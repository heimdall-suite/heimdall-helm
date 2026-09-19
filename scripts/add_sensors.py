Import("env")

# Selects exactly one IMU chip implementation per board (icm42688p.c:
# matek_h743, issue #26; mpu6500.c: afroflight32, issue #27; stub.c:
# any board without a real driver yet) -- same explicit-source-selection
# reasoning as add_bootloader.py/add_usb_cdc.py, since lib/sensors/ has
# (or will have) more than one file implementing imu_chip.h's same
# function names. lib_ignore = sensors is set in every env in
# platformio.ini; every env adds this script to their extra_scripts,
# since every board has at least the stub.c fallback.
#
# imu.c itself is NOT one of the chip-specific choices -- it's the
# board/chip-agnostic task/queue plumbing (issue #14) that calls
# whichever chip file custom_helm_imu names, so it's compiled
# unconditionally here alongside that one selection, same as
# sensors.h/imu.h/imu_chip.h being found via this script's own CPPPATH
# addition rather than PlatformIO's default lib/ include scan.
project_dir = env.subst("$PROJECT_DIR")
sensors_dir = project_dir + "/lib/sensors"

# Absent entirely (rather than every board naming "stub" explicitly) is
# read the same way custom_helm_stm32cube_package's absence is in
# add_usb_cdc.py: "no real driver for this board yet," not a missing
# option.
imu_chip = env.GetProjectOption("custom_helm_imu", "stub")

env.Append(CPPPATH=[sensors_dir])

# imu.c genuinely depends on lib/supervisor/ (it registers with the
# module-liveness supervisor, same as every other module's task) -- when
# imu.c was part of PlatformIO's normal LDF auto-scan, LDF's own chain-
# following added supervisor's directory to CPPPATH automatically by
# parsing imu.c's #include line itself. Now that lib/sensors/ is
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
    ],
)
