libvi_srcs = files(
    'vi.c'
)

libvi = library('vi',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libvi_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libvi
