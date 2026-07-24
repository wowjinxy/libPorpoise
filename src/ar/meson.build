libar_srcs = files(
    'ar.c',
    'arq.c'
)

libar = library('ar',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libar_srcs],
    native: true
)

libporpoise_libs += libar
