libdemo_srcs = files(
    'DEMOFont.cpp',
    'DEMOInit.cpp',
    'DEMOPad.cpp',
    'DEMOPuts.cpp',
    'DEMOStats.cpp',
    'DEMOTpl.c',
    'DEMOWin.cpp'
)

libdemo = library('demo',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libdemo_srcs],
    native: true
)

libporpoise_libs += libdemo
