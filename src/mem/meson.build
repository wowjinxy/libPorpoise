libmem_srcs = files(
    'MEM.c'
)

libmem = library('mem',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libmem_srcs],
    native: true
)

libporpoise_libs += libmem
