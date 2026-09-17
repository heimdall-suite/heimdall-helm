Import("env")

project_dir = env.subst("$PROJECT_DIR")
board_target = env.GetProjectOption("custom_helm_board")
board_dir = project_dir + "/boards/" + board_target

# boards/<target>/ is a sibling of src/, not nested inside it, so
# build_src_filter (which only scopes src_dir) can't reach it -- same
# reasoning as vendor/freertos-kernel's explicit env.BuildSources() in
# add_freertos.py. This is also what keeps every other board's board.c out
# of this env's build: each board.c only gets compiled by the one
# extra_script invocation whose custom_helm_board matches it.
env.Append(CPPPATH=[board_dir])

env.BuildSources(
    "$BUILD_DIR/Board",
    board_dir,
    src_filter=["-<*>", "+<*.c>"],
)
