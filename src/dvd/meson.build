libdvd_srcs = files(
    'dvd.c',
    'dvderror.c',
    'dvdfs.c',
    'dvdlow.c',
    'dvdqueue.c',
	'fstload.c'
)

libdvd = library('dvd',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libdvd_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libdvd
