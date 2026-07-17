libvi_srcs = files(
    'vi.c'
)

libvi = library('vi',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libvi_srcs],
    native: true
)

libporpoise_libs += libvi
