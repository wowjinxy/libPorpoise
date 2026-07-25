libruntime_srcs = files(
    '__init_cpp_exceptions.cpp',
    '__mem.c',
    '__va_arg.c',
    'CPlusLibPPC.cp',
    'ExceptionPPC.cp',
    'global_destructor_chain.c',
    'NMWException.cp',
    'ptmf.c',
    'runtime.c'
)

libruntime = library('Runtime',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libruntime_srcs],
    native: false
)

libporpoise_libs += libruntime
