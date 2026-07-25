libamcexi2_srcs = files(
    'AmcExi.c',
    'AmcExi2Comm.c'
)

libamcexi2 = library('ar',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libamcexi2_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libamcexi2
