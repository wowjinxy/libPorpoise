project('libporpoise', 'c', 'cpp',
    default_options: {
        'default_library': 'static',
        'prefer_static':    false
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
    native: build_target != 'gc'
)

add_project_arguments(shared_args,
    language: 'cpp',
    native: build_target != 'gc'
)

libporpoise_shared_args = [
]

if(build_target != 'gc')
libporpoise_shared_args += ['-g']
endif

if(get_option('address_sanitizer') == true)
libporpoise_shared_args += [
    '-fsanitize=address'
]
endif

libporpoise_c_args = [
]

libporpoise_cpp_args = [
]

if(build_target == 'gc') 
    libporpoise_shared_args += [
        # Platform Definitions
        '-nodefaults',
        '-nostdinc',
        '-nosyspath',
        '-proc',
        'gekko',
        '-align',
        'powerpc',
        '-enum',
        'int',
        # Multibyte Definitions
        '-multibyte',
        '-char',
        'unsigned',
        # Gekko Specific Definitions
        '-fp',
        'hardware',
        #'-Cpp_exceptions off',
        #'-pragma "cats off"',
    ]
endif

if (build_target == 'win64')
    libporpoise_shared_args += [
        '-DLIBPORPOISE_PORT',
        '-DLIBPORPOISE_BUILD_WIN64',
        '-w'
    ]
endif

if (build_target == 'linux')
    libporpoise_shared_args += [
        '-DLIBPORPOISE_PORT',
        '-DLIBPORPOISE_BUILD_LINUX',
        '-D_POSIX_C_SOURCE=200112L',
        '-w'
    ]
endif

libporpoise_c_args += libporpoise_shared_args
libporpoise_cpp_args += libporpoise_shared_args

if(build_target != 'gc')
libporpoise_c_args += [
    '-std=c99'
]

libporpoise_cpp_args += [
    '-std=c++20'
]
endif

add_project_arguments(libporpoise_c_args,
    language: 'c',
    native: build_target != 'gc'
)

add_project_arguments(libporpoise_cpp_args,
    language: 'cpp',
    native: build_target != 'gc'
)

############################################################
###                    INCLUDE PATHS                     ###
############################################################
libporpoise_include_dirs = ['include']
if(build_target == 'gc')
libporpoise_include_dirs += [
    'include/PowerPC_EABI_Support',
    'include/PowerPC_EABI_Support/MSL_C/libc'
]
endif

libporpoise_includes = include_directories(libporpoise_include_dirs)

############################################################
###                    DEPENDENCIES                      ###
############################################################
libporpoise_dependencies = []

if(build_target == 'gc')

else
    sdl2_lib = meson.get_compiler('c').find_library('SDL2')
    sdl2_dep = declare_dependency(dependencies: [sdl2_lib])
    libporpoise_dependencies += sdl2_dep
endif


############################################################
###                      LIBRARIES                       ###
############################################################
subdir('src')


############################################################
###                    FOR CONSUMERS                     ###
############################################################

libporpoise_dep = declare_dependency(
    compile_args: libporpoise_shared_args,
    include_directories: libporpoise_includes,
    link_with: libporpoise_libs + libporpoise_prebuilt,
    dependencies: libporpoise_dependencies
)

meson.override_dependency('libPorpoise', libporpoise_dep)

standalone = get_option('standalone')

if(standalone) 
    subdir('standalone')
endif