libmtx_srcs = files(
    'mtx.c',
	'mtx44.c',
	'vec.c'
)

libmtx = library('mtx',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libmtx_srcs],
    native: true
)

libporpoise_libs += libmtx
