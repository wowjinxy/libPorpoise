sim_gx_vertex_decoder_test = executable(
    'sim-gx-vertex-decoder-test',
    sources: files(
        'sim_gx_VertexDecoderTests.cpp',
        '../src/sim/sim_gx_State.cpp',
        '../src/sim/sim_gx_VertexDecoder.cpp',
    ),
    include_directories: libporpoise_includes,
    native: true,
)

test('sim-gx-vertex-decoder', sim_gx_vertex_decoder_test)

gx_wii_compatibility_test = executable(
    'gx-wii-compatibility-test',
    sources: files('gx_WiiCompatibilityTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [
        libar,
        libbase,
        libcard,
        libdb,
        libdemo,
        libdvd,
        libexi,
        libgd,
        libgx,
        libmem,
        libmtx,
        libos,
        libpad,
        libsi,
        libsim,
        libvi,
    ],
    dependencies: sdl2_dep,
    native: true,
)

test('gx-wii-compatibility', gx_wii_compatibility_test)

dvd_host_filesystem_test = executable(
    'dvd-host-filesystem-test',
    sources: files('dvd_HostFilesystemTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [
        libar,
        libbase,
        libcard,
        libdb,
        libdemo,
        libdvd,
        libexi,
        libgd,
        libgx,
        libmem,
        libmtx,
        libos,
        libpad,
        libsi,
        libsim,
        libvi,
    ],
    dependencies: sdl2_dep,
    native: true,
)

test('dvd-host-filesystem', dvd_host_filesystem_test)
