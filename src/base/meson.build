libbase_srcs = files(
    'PPCArch.c'
)

libbase = library('base',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libbase_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libbase
