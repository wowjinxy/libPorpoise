libos_srcs = files(
    'init/__ppc_eabi_init.c',
    'init/__start.c',
    'OS.c',
    'OSAlarm.c',
    'OSAlloc.c',
    'OSCache.c',
    'OSContext.c',
    'OSError.c',
    'OSFastCast.c',
    'OSFont.c',
    'OSInterrupt.c',
    'OSMutex.c',
    'OSReset.c',
    'OSResetSW.c',
    'OSRtc.c',
    'OSSemaphore.c',
    'OSStopwatch.c',
    'OSThread.cpp',
    'OSTime.c',
    'OSUart.c'
)

libos = library('os',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libos_srcs],
    native: true
)

libporpoise_libs += libos
