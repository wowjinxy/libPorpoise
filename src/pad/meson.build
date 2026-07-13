libpad_srcs = files(
    'PAD.c',
    'PADClamp.c',
    'PADConfig.c'
)

libpad = library('pad',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libpad_srcs],
    native: true
)

libporpoise_libs += libpad
