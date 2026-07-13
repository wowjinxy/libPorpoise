libsi_srcs = files(
    'SI.c'
)

libsi = library('si',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libsi_srcs],
    native: true
)

libporpoise_libs += libsi
