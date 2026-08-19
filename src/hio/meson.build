libhio_srcs = files(
    'hio.c'
)

libhio = library('hio',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libhio_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libhio
