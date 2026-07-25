libdb_srcs = files(
    'db.c'
)

libdb = library('db',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libdb_srcs],
    native: build_target != 'gc'
)

libporpoise_libs += libdb
