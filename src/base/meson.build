libbase_srcs = files(
    'PPCArch.c',
    'PPCPm.c'
)

libbase = library('base',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libbase_srcs],
    native: true
)

libporpoise_libs += libbase
