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

vi_host_retrace_test = executable(
    'vi-host-retrace-test',
    sources: files('vi_HostRetraceTests.cpp'),
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

test('vi-host-retrace', vi_host_retrace_test)

os_host_thread_queue_test = executable(
    'os-host-thread-queue-test',
    sources: files('os_HostThreadQueueTests.cpp'),
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

test('os-host-thread-queue', os_host_thread_queue_test)

os_host_message_queue_test = executable(
    'os-host-message-queue-test',
    sources: files('os_HostMessageQueueTests.cpp'),
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

test(
    'os-host-message-queue',
    os_host_message_queue_test,
    timeout: 30,
)

arc_archive_paths_test = executable(
    'arc-archive-paths-test',
    sources: files('arc_ArchivePathTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libarc],
    native: true,
)

test('arc-archive-paths', arc_archive_paths_test)

arc_external_storage_test = executable(
    'arc-external-storage-test',
    sources: files('arc_ExternalStorageTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libarc],
    native: true,
)

test('arc-external-storage', arc_external_storage_test)

arc_directory_traversal_test = executable(
    'arc-directory-traversal-test',
    sources: files('arc_DirectoryTraversalTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libarc],
    native: true,
)

test('arc-directory-traversal', arc_directory_traversal_test)

cnt_demo_cases = [
    ['023-cntdemo', '23'],
    ['024-datatitledemo', '24'],
    ['025-strapcntdemo', '25'],
]

foreach cnt_demo_case : cnt_demo_cases
    cnt_demo_test = executable(
        'sdk-' + cnt_demo_case[0] + '-test',
        sources: files('cnt_DemoEquivalentTests.cpp'),
        include_directories: libporpoise_includes,
        cpp_args: ['-DSDK_CNT_DEMO_CASE=' + cnt_demo_case[1]],
        link_with: [libcnt],
        native: true,
    )
    test('sdk-' + cnt_demo_case[0], cnt_demo_test)
endforeach

cx_demo_cases = [
    ['026-cx-uncompress', '26'],
    ['027-cx-uncompress-stream', '27'],
]

foreach cx_demo_case : cx_demo_cases
    cx_demo_test = executable(
        'sdk-' + cx_demo_case[0] + '-test',
        sources: files('cx_DemoEquivalentTests.cpp'),
        include_directories: libporpoise_includes,
        cpp_args: ['-DSDK_CX_DEMO_CASE=' + cx_demo_case[1]],
        link_with: [libcx],
        native: true,
    )
    test('sdk-' + cx_demo_case[0], cx_demo_test)
endforeach

darch_demo_test = executable(
    'sdk-028-darchdemo-test',
    sources: files('darch_DemoEquivalentTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libdarch, libarc],
    native: true,
)

test('sdk-028-darchdemo', darch_demo_test)

dvd_sdk_link_libraries = [
    libbase,
    libdb,
    libdvd,
    libexi,
    libos,
    libpad,
    libsi,
    libvi,
]

dvd_directory_demo_test = executable(
    'sdk-029-dvd-directory-test',
    sources: files(
        'dvd_DirectoryDemoEquivalentTests.cpp',
        '../src/sim/sim_globals.c',
        'sdk_DvdRuntimeStubs.c',
    ),
    include_directories: libporpoise_includes,
    link_with: dvd_sdk_link_libraries,
    dependencies: sdl2_dep,
    native: true,
)

test('sdk-029-dvd-directory', dvd_directory_demo_test, is_parallel: false)

dvd_synchronous_read_demo_test = executable(
    'sdk-030-dvddemo1-test',
    sources: files(
        'dvd_SynchronousReadDemoEquivalentTests.cpp',
        '../src/sim/sim_globals.c',
        'sdk_DvdRuntimeStubs.c',
    ),
    include_directories: libporpoise_includes,
    link_with: dvd_sdk_link_libraries,
    dependencies: sdl2_dep,
    native: true,
)

test('sdk-030-dvddemo1', dvd_synchronous_read_demo_test, is_parallel: false)
