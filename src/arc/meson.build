libarc_srcs = files(
    'arc.c'
)

libarc = library('arc',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libarc_srcs],
    native: true
)

libporpoise_libs += libarc
