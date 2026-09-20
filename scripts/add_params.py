Import("env")

# Selects exactly one flash backend per board (stm32h7.c for matek_h743,
# stm32f1.c for afroflight32) alongside the always-compiled, chip-
# agnostic generic store (params.c) -- same "shared file(s) + one
# explicit chip pick" shape as add_sensors.py's imu.c/baro.c + chip file,
# not add_bootloader.py's single-file-only shape, since params.c's own
# registry/magic/CRC/cache logic is genuine shared logic, not itself a
# chip choice. lib_ignore = params is set in every env in platformio.ini;
# only boards with HELM_FEATURE_PARAMS_PERSIST actually add this script
# (same "board must opt in" reasoning as add_bootloader.py) -- unlike
# sensors, nothing calls into this module unconditionally from CLI diag
# code, so there's no need for a stub.c fallback the way imu/baro have
# one.
project_dir = env.subst("$PROJECT_DIR")
params_backend = env.GetProjectOption("custom_helm_params_backend")
params_dir = project_dir + "/lib/params"

env.Append(CPPPATH=[params_dir])

env.BuildSources(
    "$BUILD_DIR/Params",
    params_dir,
    src_filter=[
        "-<*>",
        "+<params.c>",
        "+<" + params_backend + ".c>",
    ],
)
