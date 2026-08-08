libsim_srcs = files(
    'glad.c',
    'sim_dvd.cpp',
    'sim_main.cpp',
    'sim_globals.c',
    'sim_gx_CommandProcessor.cpp',
    'sim_gx_GlRenderer.cpp',
    'sim_gx_Geometry.cpp',
    'sim_gx_Shader.cpp',
    'sim_gx_State.cpp',
    'sim_gx_TextureManager.cpp',
    'sim_pad.cpp'
)

libsim_dependencies = []
libsim_includes = [libporpoise_includes]
libsim_args = []

if(get_option('tracy_enable'))
tracy_dep = dependency(
    'tracy', 
    default_options: {
        'no_sampling': true,
        'only_ipv4': true,
        'delayed_init': true,
        'manual_lifetime': true,
        'no_callstack': true,
        'timer_fallback': true,
    },
    static : true,
    native : true)
libsim_dependencies += tracy_dep

libsim_includes += [subproject('tracy').get_variable('tracy_public_include_dirs')]
libsim_args += [
    '-DTRACY_ENABLE'
]

if(build_target == 'win64')
ws2_32_lib = meson.get_compiler('c').find_library('ws2_32')
ws2_32_dep = declare_dependency(dependencies: [ws2_32_lib])
libsim_dependencies += ws2_32_dep
endif
endif

libsim = library('sim',
    include_directories: libporpoise_includes,
    pic: false,
    dependencies: libsim_dependencies,
    c_args: libsim_args,
    cpp_args: libsim_args,
    sources: [libsim_srcs],
    native: true
)

libporpoise_libs += libsim
