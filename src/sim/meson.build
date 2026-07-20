libsim_srcs = files(
    'glad.c',
    'sim_main.cpp',
    'sim_globals.c',
    'sim_gpu.cpp'
)

libsim = library('sim',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libsim_srcs],
    native: true
)

libporpoise_libs += libsim
