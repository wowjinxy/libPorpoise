libar_srcs = files(
    'AR.c',
    'ARQ.c'
)

libar = library('ar',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libar_srcs],
    native: true
)

libporpoise_libs += libar
