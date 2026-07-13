libcard_srcs = files(
    'CARDBios.c',
    'CARDBlock.c',
    'CARDCheck.c',
    'CARDCreate.c',
    'CARDDelete.c',
    'CARDFormat.c',
    'CARDMount.c',
    'CARDOpen.c',
    'CARDRead.c',
    'CARDRename.c',
    'CARDStat.c',
    'CARDWrite.c'
)

libcard = library('card',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libcard_srcs],
    native: true
)

libporpoise_libs += libcard
