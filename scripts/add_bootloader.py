Import("env")

# Selects exactly one bootloader-jump implementation per board (only
# stm32h7.c exists so far), same explicit-source-selection reasoning as
# add_rx.py: lib/bootloader/ has (or will have) more than one chip-specific
# .c file implementing bootloader.h's same function names, so leaving it to
# PlatformIO's default lib/ auto-scan would try to compile all of them into
# every board and collide at link time. lib_ignore = bootloader is set in
# every env in platformio.ini; only boards that actually have an
# implementation add this script to their extra_scripts.
project_dir = env.subst("$PROJECT_DIR")
bootloader_chip = env.GetProjectOption("custom_helm_bootloader")
bootloader_dir = project_dir + "/lib/bootloader"

env.Append(CPPPATH=[bootloader_dir])

env.BuildSources(
    "$BUILD_DIR/Bootloader",
    bootloader_dir,
    src_filter=[
        "-<*>",
        "+<" + bootloader_chip + ".c>",
    ],
)
