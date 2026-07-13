libdb_srcs = files(
    'db.c',
    'dbprintf.c'
)

libdb = library('db',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libdb_srcs],
    native: true
)

libporpoise_libs += libdb
