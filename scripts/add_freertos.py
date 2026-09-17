Import("env")

project_dir = env.subst("$PROJECT_DIR")
freertos_dir = project_dir + "/vendor/freertos-kernel"
port_dir = freertos_dir + "/portable/GCC/ARM_CM7/r0p1"

env.Append(
    CPPPATH=[
        freertos_dir + "/include",
        port_dir,
        project_dir + "/config",
    ],
    LINKFLAGS=[
        "-mfpu=fpv5-d16",
        "-mfloat-abi=hard",
    ],
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
        "+<portable/GCC/ARM_CM7/r0p1/*.c>",
        "+<portable/MemMang/heap_4.c>",
    ],
)
