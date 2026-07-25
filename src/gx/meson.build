libgx_srcs = files(
    'GXAttr.c',
	'GXBump.c',
	'GXDisplayList.c',
	'GXFifo.c',
	'GXFrameBuf.c',
	'GXGeometry.c',
	'GXInit.c',
	'GXLight.c',
	'GXMisc.c',
	'GXPerf.c',
	'GXPixel.c',
	'GXStubs.c',
	'GXTev.c',
	'GXTexture.c',
	'GXTransform.c'
)

libgx = library('gx',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libgx_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libgx
