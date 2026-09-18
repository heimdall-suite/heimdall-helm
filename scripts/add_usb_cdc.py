Import("env")

# Selects exactly one USB CDC transport implementation per board (only
# stm32h7.c exists so far) -- same explicit-source-selection reasoning as
# add_bootloader.py. lib_ignore = usb_cdc is set in every env in
# platformio.ini; only boards that actually have an implementation add
# this script to their extra_scripts.
project_dir = env.subst("$PROJECT_DIR")
usb_cdc_chip = env.GetProjectOption("custom_helm_usb_cdc")
usb_cdc_dir = project_dir + "/lib/usb_cdc"

env.Append(CPPPATH=[usb_cdc_dir])

env.BuildSources(
    "$BUILD_DIR/UsbCdc",
    usb_cdc_dir,
    src_filter=[
        "-<*>",
        "+<" + usb_cdc_chip + ".c>",
    ],
)

# ST's USB Device middleware (Middlewares/ST/STM32_USB_Device_Library/...)
# ships with the stm32cube framework package, but stm32cube.py only
# registers it as a *candidate* library for PlatformIO's LDF chain-scan
# (build_usb_libs() in that platform's builder/frameworks/stm32cube.py) --
# it never actually gets compiled unless the LDF itself discovers a
# #include for it while walking files it scans. Our own USB code
# (lib/usb_cdc/stm32h7.c, boards/<target>/usbd_conf.c/usbd_desc.c) is
# added via explicit env.BuildSources() instead, the same
# bypass-the-LDF-entirely pattern boards/, vendor/freertos-kernel/, and
# lib/bootloader/ already use -- so the LDF never sees those #include
# lines and never pulls the middleware in on its own. Compile the two
# pieces this project actually uses (Core, Class/CDC) explicitly here,
# same as everything else that pattern touches.
platform = env.PioPlatform()
usb_lib_dir = platform.get_package_dir("framework-stm32cubeh7") + "/Middlewares/ST/STM32_USB_Device_Library"

env.Append(
    CPPPATH=[
        usb_lib_dir + "/Core/Inc",
        usb_lib_dir + "/Class/CDC/Inc",
    ]
)

env.BuildSources(
    "$BUILD_DIR/UsbDeviceLibCore",
    usb_lib_dir + "/Core/Src",
    src_filter=["-<*>", "+<*.c>", "-<*_template.c>"],
)

env.BuildSources(
    "$BUILD_DIR/UsbDeviceLibCdc",
    usb_lib_dir + "/Class/CDC/Src",
    src_filter=["-<*>", "+<*.c>", "-<*_template.c>"],
)
