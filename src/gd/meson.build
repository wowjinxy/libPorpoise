libgd_srcs = files(
    'GDBase.c',
    'GDFile.c',
    'GDGeometry.c',
    'GDIndirect.c',
    'GDLight.c',
    'GDPixel.c',
    'GDTev.c',
    'GDTexture.c',
    'GDTransform.c'
)

libgd = library('gd',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libgd_srcs],
    c_args: ['-DWIN32'],
    native: true
)

libporpoise_libs += libgd
