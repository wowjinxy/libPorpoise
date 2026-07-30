libgba_srcs = files(
    'GBA.c',
	'GBAGetProcessStatus.c',
    'GBAJoyBoot.c',
    'GBARead.c',
    'GBAWrite.c',
    'GBAXfer.c'
)

libgba = library('gba',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libgba_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libgba
