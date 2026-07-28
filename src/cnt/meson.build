libcnt_srcs = files(
    'cnt.cpp'
)

libcnt = library('cnt',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libcnt_srcs],
    native: true,
)

libporpoise_libs += libcnt
