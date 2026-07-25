libsi_srcs = files(
    'SIBios.c',
	'SISamplingRate.c'
)

libsi = library('si',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libsi_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libsi
