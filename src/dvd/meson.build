libdvd_srcs = files(
    'DVD.c',
    'DVDError.c',
    'DVDFatal.c',
    'DVDLow.c',
    'DVDQueue.c'
)

libdvd = library('dvd',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libdvd_srcs],
    native: true
)

libporpoise_libs += libdvd
