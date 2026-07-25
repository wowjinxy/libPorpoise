libamcnotstub_srcs = files(
    'amcnotstub.c'
)

libamcnotstub = library('ar',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libamcnotstub_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libamcnotstub
