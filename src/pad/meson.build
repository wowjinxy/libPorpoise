libpad_srcs = files(
    'Pad.c',
    'Padclamp.c'
)

libpad = library('pad',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libpad_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libpad
