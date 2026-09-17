Import("env")

project_dir = env.subst("$PROJECT_DIR")
freertos_dir = project_dir + "/vendor/freertos-kernel"

# Per-env: which Cortex-M port + FPU flags. Set by each [env:...] in
# platformio.ini via custom_helm_freertos_port / custom_helm_fpu_flags --
# e.g. "ARM_CM7/r0p1" + "-mfpu=fpv5-d16 -mfloat-abi=hard" for the H743
# (double-precision FPU), "ARM_CM3" + "" for the Afroflight32 (no FPU).
#
# custom_helm_fpu_flags is the SINGLE source of truth for these flags --
# applied to both CCFLAGS (needed to compile the FreeRTOS port itself,
# which #errors at compile time without hardware float enabled) and
# LINKFLAGS (needed because these -m flags aren't otherwise propagated to
# the final link step, causing "uses VFP register arguments" mismatches
# against object files that don't have them -- both bench-confirmed during
# initial bring-up). Do NOT also list -mfpu/-mfloat-abi in this env's own
# build_flags in platformio.ini -- that duplication is exactly what caused
# nexus_xr to silently drift out of sync with matek_h743 the first time
# this was set up (fixed once, here, instead).
freertos_port = env.GetProjectOption("custom_helm_freertos_port")
fpu_flags = env.GetProjectOption("custom_helm_fpu_flags", default="").split()
port_dir = freertos_dir + "/portable/GCC/" + freertos_port

env.Append(
    CPPPATH=[
        freertos_dir + "/include",
        port_dir,
    ],
    CCFLAGS=fpu_flags,
    LINKFLAGS=fpu_flags,
)

env.BuildSources(
    "$BUILD_DIR/FreeRTOSKernel",
    freertos_dir,
    src_filter=[
        "-<*>",
        "+<tasks.c>",
        "+<queue.c>",
        "+<list.c>",
        "+<timers.c>",
        "+<croutine.c>",
        "+<event_groups.c>",
        "+<stream_buffer.c>",
        "+<portable/GCC/" + freertos_port + "/*.c>",
        "+<portable/MemMang/heap_4.c>",
    ],
)
