libdsp_srcs = files(
    'dsp.c'
)

if build_target != 'gc'
libdsp_srcs += files('dsp_host_init.c')
endif

libdsp = library('dsp',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libdsp_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libdsp
