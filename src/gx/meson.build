libgx_srcs = files(
    'G2D.c',
	'GXAttr.c',
	'GXBump.c',
	'GXData.c',
	'GXDisplayList.c',
	'GXDraw.c',
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
    native: true
)

libporpoise_libs += libgx
