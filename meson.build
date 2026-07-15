project('libporpoise', 'c', 'cpp',
    default_options: {
        'default_library': 'static',
        'prefer_static':    true
    },
    meson_version: '>=1.2.0',
    version: '1.0'
)

fs = import('fs')

buildtype = get_option('buildtype')
cc = meson.get_compiler('c')
build_target = get_option('build_target')

############################################################
###                    COMPILER FLAGS                    ###
############################################################
shared_args = []

add_project_arguments(shared_args,
    language: 'c',
    native: true
)

add_project_arguments(shared_args,
    language: 'cpp',
    native: true
)

libporpoise_shared_args = [
]

libporpoise_c_args = [
]

libporpoise_cpp_args = [
]


if (build_target == 'win64')
    libporpoise_shared_args += [
        '-DLIBPORPOISE_BUILD_WIN64',
        '-w'
    ]
endif

if (build_target == 'linux')
    libporpoise_shared_args += [
        '-DLIBPORPOISE_BUILD_LINUX',
        '-D_POSIX_C_SOURCE=200112L',
        '-w'
    ]
endif

libporpoise_c_args += libporpoise_shared_args
libporpoise_c_args += [
    '-std=c99'
]

libporpoise_cpp_args += libporpoise_shared_args
libporpoise_cpp_args += [
    '-std=c++20'
]

add_project_arguments(libporpoise_c_args,
    language: 'c',
    native: true
)

add_project_arguments(libporpoise_cpp_args,
    language: 'cpp',
    native: true
)

############################################################
###                    INCLUDE PATHS                     ###
############################################################
libporpoise_includes = include_directories('include')

############################################################
###                      LIBRARIES                       ###
############################################################
subdir('src')
sdl2_lib = meson.get_compiler('c').find_library('SDL2')
sdl2_dep = declare_dependency(dependencies: [sdl2_lib])

############################################################
###                    FOR CONSUMERS                     ###
############################################################

libporpoise_dep = declare_dependency(
    compile_args: libporpoise_shared_args,
    include_directories: libporpoise_includes,
    link_with: libporpoise_libs + libporpoise_prebuilt,
    dependencies: sdl2_dep
)

meson.override_dependency('libporpoise', libporpoise_dep)

standalone = get_option('standalone')

if(standalone) 
    subdir('standalone')
endif