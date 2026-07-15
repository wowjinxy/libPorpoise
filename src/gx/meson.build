libgx_srcs = files(
    'GXApi.cpp'
)

libgx = library('gx',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libgx_srcs],
    native: true
)

libporpoise_libs += libgx
