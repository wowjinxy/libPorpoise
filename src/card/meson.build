libcard_srcs = files(
    'CARDBios.c',
    'CARDBlock.c',
    'CARDCheck.c',
    'CARDCreate.c',
    'CARDDelete.c',
	'CARDDir.c',
    'CARDFormat.c',
    'CARDMount.c',
	'CARDNet.c',
    'CARDOpen.c',
	'CARDRdwr.c',
    'CARDRead.c',
    'CARDRename.c',
    'CARDStat.c',
	'CARDStatEx.c',
	'CARDUnlock.c',
    'CARDWrite.c'
)

libcard = library('card',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libcard_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libcard
