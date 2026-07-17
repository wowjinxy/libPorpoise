libexi_srcs = files(
    'EXIBios.c',
	'EXIUart.c'
)

libexi = library('exi',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libexi_srcs],
    native: true
)

libporpoise_libs += libexi
