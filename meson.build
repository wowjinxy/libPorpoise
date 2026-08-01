project('libporpoise', 'c', 'cpp',
    default_options: {
        'default_library': 'static',
        'prefer_static':    false
    },
    meson_version: '>=1.2.0',
    version: '1.0'
)

fs = import('fs')
codewarrior_source_compat = find_program(
    'tools/codewarrior_source_compat.py',
    native: true,
)
pe_static_u32_fixups = find_program(
    'tools/pe_static_u32_fixups.py',
    native: true,
)

buildtype = get_option('buildtype')
cc = meson.get_compiler('c')
build_target = get_option('build_target')
host_memory_profile = get_option('host_memory_profile')
libporpoise_platform_dependencies = []

# Win64 consumers need the DLLs which back libPorpoise's SDL and MinGW
# dependencies beside their executable.  Export both the File objects and
# their destination names so each consumer can stage them in its own output
# directory without duplicating toolchain-specific knowledge.
libporpoise_win64_runtime_files = []
libporpoise_win64_runtime_basenames = []

if build_target == 'win64'
    libporpoise_win64_runtime_basenames = [
        'SDL2.dll',
        'libgcc_s_seh-1.dll',
        'libstdc++-6.dll',
        'libwinpthread-1.dll',
    ]

    foreach runtime_basename : libporpoise_win64_runtime_basenames
        libporpoise_win64_runtime_files += files(
            'msys2/ucrt64/bin' / runtime_basename,
        )
    endforeach
endif

if build_target == 'linux'
    libporpoise_platform_dependencies += cc.find_library('dl', required: false)
endif

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

if build_target != 'gc' and host_memory_profile == 'gamecube_extended'
    libporpoise_shared_args += [
        '-DLIBPORPOISE_HOST_MEMORY_PROFILE=OS_HOST_MEMORY_PROFILE_GAMECUBE_EXTENDED',
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

libporpoise_dependencies += libporpoise_platform_dependencies


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

tests_option = get_option('tests')
build_tests = tests_option.enabled() or (tests_option.auto() and not meson.is_subproject())

if build_tests
    subdir('tests')
endif
