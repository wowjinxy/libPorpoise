libsim_srcs = files(
    'glad.c',
    'sim_main.cpp',
    'sim_globals.c',
    'sim_gx_CommandProcessor.cpp',
    'sim_gx_ColorChannel.cpp',
    'sim_gx_GlRenderer.cpp',
    'sim_gx_Geometry.cpp',
    'sim_gx_Shader.cpp',
    'sim_gx_State.cpp',
    'sim_gx_TexGen.cpp',
    'sim_gx_TextureCache.cpp',
    'sim_gx_UniformCache.cpp',
    'sim_gx_VertexDecoder.cpp',
    'sim_host_Allocator.cpp'
)

libsim = library('sim',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libsim_srcs],
    native: true
)

libporpoise_libs += libsim
