libdsp_srcs = files(
    'dsp.c'
)

libdsp = library('dsp',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libdsp_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libdsp
