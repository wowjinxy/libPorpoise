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
c_args = []

add_project_arguments(c_args,
    language: 'c',
    native: true
)

add_project_arguments(c_args,
    language: 'cpp',
    native: true
)

libporpoise_args = [
]

if (build_target == 'win64')
    libporpoise_args += [
        '-DHW2',
        '-DGEKKO',
        '-DGX_REV=2',
        '-DLIBPORPOISE_BUILD_WIN64',
        '-w'
    ]
endif

if (build_target == 'linux')
    libporpoise_args += [
        '-DHW2',
        '-DGEKKO',
        '-DGX_REV=2',
        '-DLIBPORPOISE_BUILD_LINUX',
        '-w'
    ]
endif

add_project_arguments(libporpoise_args,
    language: 'c',
    native: true
)

add_project_arguments(libporpoise_args,
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

############################################################
###                    FOR CONSUMERS                     ###
############################################################

libporpoise_dep = declare_dependency(
    compile_args: libporpoise_args,
    include_directories: libporpoise_includes,
    link_with: libporpoise_libs + libporpoise_prebuilt
)

meson.override_dependency('libporpoise', libporpoise_dep)
