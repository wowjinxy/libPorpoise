libmetrotrk_srcs = files(
    '__exception.c',
    'dispatch.c',
    'dolphin_trk.c',
    'dolphin_trk_glue.c',
    'flush_cache.c',
    'main_TRK.c',
    'mainloop.c',
    'mem_TRK.c',
    'mpc_7xx_603e.c',
    'msg.c',
    'msgbuf.c',
    'msghndlr.c',
    'mutex_TRK.c',
    'notify.c',
    'nubevent.c',
    'nubinit.c',
    'serpoll.c',
    'support.c',
    'targcont.c',
    'targimpl.c',
    'usr_put.c'
)

libmetrotrk = library('MetroTRK',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libmetrotrk_srcs],
    native: false
)

libporpoise_libs += libmetrotrk
