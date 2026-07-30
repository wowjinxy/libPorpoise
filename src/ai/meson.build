libai_srcs = files(
    'ai.c'
)

libai = library('ai',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libai_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libai
