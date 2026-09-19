Import("env")

# Selects exactly one usb_cdc transport implementation per board
# (stm32h7.c: native OTG_FS USB CDC; stm32f1.c: a plain UART bridged to
# an onboard USB-serial converter chip, not native USB at all -- see
# that file's own header comment) -- same explicit-source-selection
# reasoning as add_bootloader.py. lib_ignore = usb_cdc is set in every
# env in platformio.ini; only boards that actually have an implementation
# add this script to their extra_scripts.
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

# Not every usb_cdc implementation is a native USB device: afroflight32's
# (#12) is a plain UART bridged to an onboard USB-serial converter chip
# in external hardware (confirmed against aoa-boat-controller's real
# firmware for that exact board -- see lib/usb_cdc/stm32f1.c's own header
# comment), so it has nothing to do with ST's USB Device middleware at
# all. custom_helm_stm32cube_package is only set in platformio.ini for
# envs whose usb_cdc implementation actually needs that middleware
# (matek_h743's native OTG_FS device) -- its absence here means "this
# board's transport isn't native USB," not a missing/forgotten option, so
# skip the rest of this script entirely rather than erroring.
stm32cube_package = env.GetProjectOption("custom_helm_stm32cube_package", None)
if stm32cube_package is not None:
    # ST's USB Device middleware (Middlewares/ST/STM32_USB_Device_Library/
    # ...) ships with the stm32cube framework package, but stm32cube.py
    # only registers it as a *candidate* library for PlatformIO's LDF
    # chain-scan (build_usb_libs() in that platform's builder/frameworks/
    # stm32cube.py) -- it never actually gets compiled unless the LDF
    # itself discovers a #include for it while walking files it scans.
    # Our own USB code (lib/usb_cdc/<chip>.c, boards/<target>/
    # usbd_conf.c/usbd_desc.c) is added via explicit env.BuildSources()
    # instead, the same bypass-the-LDF-entirely pattern boards/,
    # vendor/freertos-kernel/, and lib/bootloader/ already use -- so the
    # LDF never sees those #include lines and never pulls the middleware
    # in on its own. Compile the two pieces this project actually uses
    # (Core, Class/CDC) explicitly here, same as everything else that
    # pattern touches.
    #
    # The middleware itself is per-chip-FAMILY (framework-stm32cubeh7 vs
    # .../f1 vs .../f7), hence this being a per-env option rather than a
    # hardcoded path -- originally hardcoded to "framework-stm32cubeh7"
    # (the only chip this had ever been ported to), which would have
    # silently resolved to the wrong package the moment a second chip
    # family's usb_cdc implementation needed this script (#12 turned out
    # to be a UART bridge instead, so that specific breakage never
    # actually landed -- but the fragility it would have hit is exactly
    # why this stayed a named option instead of being hardcoded again).
    platform = env.PioPlatform()
    usb_lib_dir = platform.get_package_dir(stm32cube_package) + "/Middlewares/ST/STM32_USB_Device_Library"

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
