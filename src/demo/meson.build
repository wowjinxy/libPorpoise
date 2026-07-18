libdemo_srcs = files(
    'DEMOAVX.c',
    'DEMOFont.c',
    'DEMOInit.c',
    'DEMOPad.c',
    'DEMOPuts.c',
    'DEMOStats.c',
    'DEMOWin.c'
)

libdemo = library('demo',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libdemo_srcs],
    native: true
)

libporpoise_libs += libdemo
