Import("env")

# Selects exactly one RX protocol driver per board (sbus.c or crsf.c),
# same explicit-source-selection reasoning as add_board.py: sbus.c and
# crsf.c implement the same function names (rx_init/rx_update/
# rx_get_latest -- two implementations of one interface), so if both were
# left to PlatformIO's default lib/ auto-scan, its LDF would try to
# compile both into every board's build and collide at link time. This is
# why lib_ignore = rx is set in every env in platformio.ini -- this script
# is the ONLY thing that compiles lib/rx/, matching how boards/ and
# vendor/freertos-kernel/ are already handled.
project_dir = env.subst("$PROJECT_DIR")
rx_protocol = env.GetProjectOption("custom_helm_rx")
rx_dir = project_dir + "/lib/rx"

env.Append(CPPPATH=[rx_dir])

env.BuildSources(
    "$BUILD_DIR/Rx",
    rx_dir,
    src_filter=[
        "-<*>",
        "+<" + rx_protocol + ".c>",
        "+<shared/*.c>",
    ],
)
