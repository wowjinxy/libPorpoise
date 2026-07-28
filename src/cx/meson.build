libcx_srcs = files(
    'cx.c'
)

libcx = library('cx',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libcx_srcs],
    native: true,
)

libporpoise_libs += libcx
