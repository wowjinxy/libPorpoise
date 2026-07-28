libos_srcs = files(
    '__ppc_eabi_init.cpp',
    '__start.c',
    'OS.c',
    'OSAlarm.c',
    'OSAlloc.c',
	'OSArena.c',
	'OSAudioSystem.c',
    'OSCache.c',
    'OSContext.c',
    'OSError.c',
    'OSFont.c',
    'OSHostMemory.c',
    'OSInterrupt.c',
	'OSLink.c',
	'OSMemory.c',
	'OSMessage.c',
    'OSMutex.c',
	'OSReboot.c',
    'OSReset.c',
    'OSResetSW.c',
    'OSRtc.c',
	'OSSync.c',
    'OSStopwatch.c',
    'OSThread.c',
    'OSTime.c'
)

libos = library('os',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libos_srcs],
    native: true
)

libporpoise_libs += libos
