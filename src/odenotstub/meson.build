libodenotstub_srcs = files(
    'odenotstub.c'
)

libodenotstub = library('ar',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libodenotstub_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libodenotstub
