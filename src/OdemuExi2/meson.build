libOdemuExi2_srcs = files(
    'DebuggerDriver.c'
)

libOdemuExi2 = library('ar',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libOdemuExi2_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libOdemuExi2
