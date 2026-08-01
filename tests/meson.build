source_compat_python = import('python').find_installation()
test(
    'codewarrior-source-compat',
    source_compat_python,
    args: [meson.current_source_dir() / 'codewarrior_SourceCompatTests.py'],
)

test(
    'pe-static-u32-fixups',
    source_compat_python,
    args: [meson.current_source_dir() / 'pe_StaticU32FixupTests.py'],
)

if build_target == 'win64'
    pe_static_u32_unfixed_fixture = executable(
        'pe-static-u32-unfixed-fixture',
        sources: files('pe_StaticU32Fixture.c'),
        include_directories: libporpoise_includes,
        link_args: [
            '-Wl,--image-base,0x10000000',
            '-Wl,--disable-dynamicbase',
            '-Wl,--disable-high-entropy-va',
        ],
        native: true,
    )

    pe_static_u32_fixed_fixture = custom_target(
        'pe-static-u32-fixed-fixture',
        input: pe_static_u32_unfixed_fixture,
        output: 'pe-static-u32-fixture.exe',
        command: [
            pe_static_u32_fixups,
            '--input', '@INPUT@',
            '--output', '@OUTPUT@',
            '--image-base', '0x10000000',
        ],
    )

    test(
        'pe-static-u32-linked-fixture',
        source_compat_python,
        args: [
            meson.current_source_dir() / 'pe_RunStaticU32Fixture.py',
            pe_static_u32_fixed_fixture,
        ],
    )
endif

sim_gx_vertex_decoder_test = executable(
    'sim-gx-vertex-decoder-test',
    sources: files(
        'sim_gx_VertexDecoderTests.cpp',
        '../src/sim/sim_gx_State.cpp',
        '../src/sim/sim_gx_TexGen.cpp',
        '../src/sim/sim_gx_TextureCache.cpp',
        '../src/sim/sim_gx_UniformCache.cpp',
        '../src/sim/sim_gx_VertexDecoder.cpp',
    ),
    include_directories: libporpoise_includes,
    native: true,
)

test('sim-gx-vertex-decoder', sim_gx_vertex_decoder_test)

sim_gx_color_channel_test = executable(
    'sim-gx-color-channel-test',
    sources: files(
        'sim_gx_ColorChannelTests.cpp',
        '../src/sim/sim_gx_ColorChannel.cpp',
        '../src/sim/sim_gx_State.cpp',
    ),
    include_directories: libporpoise_includes,
    native: true,
)

test('sim-gx-color-channel', sim_gx_color_channel_test)

sim_host_allocator_test = executable(
    'sim-host-allocator-test',
    sources: files(
        'sim_HostAllocatorTests.cpp',
        '../src/sim/sim_host_Allocator.cpp',
    ),
    include_directories: libporpoise_includes,
    dependencies: dependency('threads'),
    native: true,
)

test('sim-host-allocator', sim_host_allocator_test)

gx_fifo_color_endian_test = executable(
    'gx-fifo-color-endian-test',
    sources: files('gx_FifoColorEndianTests.cpp'),
    include_directories: libporpoise_includes,
    native: true,
)

test('gx-fifo-color-endian', gx_fifo_color_endian_test)

mtx_host_compatibility_test = executable(
    'mtx-host-compatibility-test',
    sources: files('mtx_HostCompatibilityTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libmtx],
    native: true,
)

test('mtx-host-compatibility', mtx_host_compatibility_test)

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

dvd_bb2_wire_test = executable(
    'dvd-bb2-wire-test',
    sources: files('dvd_BB2WireTests.cpp'),
    include_directories: libporpoise_includes,
    native: true,
)

test('dvd-bb2-wire', dvd_bb2_wire_test)

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

vi_host_handoff_test = executable(
    'vi-host-handoff-test',
    sources: files('vi_HostHandoffTests.cpp'),
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

test('vi-host-handoff', vi_host_handoff_test)

vi_host_owner_exit_test = executable(
    'vi-host-owner-exit-test',
    sources: files('vi_HostOwnerExitTests.cpp'),
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

test('vi-host-owner-exit', vi_host_owner_exit_test, timeout: 10)

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

os_host_interrupt_test = executable(
    'os-host-interrupt-test',
    sources: files('os_HostInterruptTests.cpp'),
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

test('os-host-interrupt', os_host_interrupt_test, timeout: 10)

os_host_alarm_test = executable(
    'os-host-alarm-test',
    sources: files('os_HostAlarmTests.cpp'),
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

test('os-host-alarm', os_host_alarm_test, timeout: 10)

os_host_memory_test = executable(
    'os-host-memory-test',
    sources: files('os_HostMemoryTests.cpp'),
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

test('os-host-memory', os_host_memory_test, timeout: 10)

os_host_memory_extended_test = executable(
    'os-host-memory-extended-test',
    sources: files('os_HostMemoryExtendedTests.cpp'),
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

test('os-host-memory-extended', os_host_memory_extended_test, timeout: 10)

os_host_address_test = executable(
    'os-host-address-test',
    sources: files('os_HostAddressTests.cpp'),
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

test('os-host-address', os_host_address_test, timeout: 20)

ar_host_compatibility_test = executable(
    'ar-host-compatibility-test',
    sources: files('ar_HostCompatibilityTests.cpp'),
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

test('ar-host-compatibility', ar_host_compatibility_test, timeout: 20)

if build_target != 'gc'
ar_allocator_test = executable(
    'ar-allocator-test',
    sources: files('ar_AllocatorTests.cpp'),
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

test('ar-allocator', ar_allocator_test, timeout: 20)

ai_host_stub_test = executable(
    'ai-host-stub-test',
    sources: files('ai_HostStubTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libai, libdsp],
    native: true,
)

test('ai-host-stub', ai_host_stub_test, timeout: 10)

hio_host_no_device_test = executable(
    'hio-host-no-device-test',
    sources: files('hio_HostNoDeviceTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libhio],
    native: true,
)

test('hio-host-no-device', hio_host_no_device_test, timeout: 10)

base_api_test = executable(
    'base-api-test',
    sources: files('base_ApiTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libbase],
    native: true,
)

test('base-api', base_api_test, timeout: 10)

dsp_override_test = executable(
    'dsp-override-test',
    sources: files('dsp_OverrideTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libdsp],
    native: true,
)

test('dsp-override', dsp_override_test, timeout: 10)

dsp_host_stub_test = executable(
    'dsp-host-stub-test',
    sources: files('dsp_HostStubTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libdsp],
    native: true,
)

test('dsp-host-stub', dsp_host_stub_test, timeout: 10)

card_api_completion_test = executable(
    'card-api-completion-test',
    sources: files('card_ApiCompletionTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libcard],
    native: true,
)

test('card-api-completion', card_api_completion_test, timeout: 10)

card_wire_endian_test = executable(
    'card-wire-endian-test',
    sources: files('card_WireEndianTests.cpp'),
    include_directories: libporpoise_includes,
    native: true,
)

test('card-wire-endian', card_wire_endian_test, timeout: 10)

native_module_fixture = shared_module(
    'porpoise-native-module-fixture',
    sources: files('os_NativeModuleFixture.c'),
    include_directories: libporpoise_includes,
    name_prefix: '',
    native: true,
)

native_module_invalid_abi_fixture = shared_module(
    'porpoise-native-module-invalid-abi-fixture',
    sources: files('os_NativeModuleInvalidAbiFixture.c'),
    include_directories: libporpoise_includes,
    name_prefix: '',
    native: true,
)

native_module_test = executable(
    'os-native-module-test',
    sources: files('os_NativeModuleTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libos],
    dependencies: libporpoise_platform_dependencies,
    native: true,
)

test(
    'os-native-module',
    native_module_test,
    depends: [native_module_fixture, native_module_invalid_abi_fixture],
)
endif

os_host_endian_test = executable(
    'os-host-endian-test',
    sources: files('os_HostEndianTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libos],
    native: true,
)

test('os-host-endian', os_host_endian_test)

os_rtc_wire_endian_test = executable(
    'os-rtc-wire-endian-test',
    sources: files('os_RtcWireEndianTests.cpp'),
    include_directories: libporpoise_includes,
    native: true,
)

test('os-rtc-wire-endian', os_rtc_wire_endian_test)

si_host_wire_test = executable(
    'si-host-wire-test',
    sources: files('si_HostWireTests.cpp'),
    include_directories: libporpoise_includes,
    native: true,
)

test('si-host-wire', si_host_wire_test)

mem_bitfield_endian_test = executable(
    'mem-bitfield-endian-test',
    sources: files('mem_BitfieldEndianTests.cpp'),
    include_directories: libporpoise_includes,
    native: true,
)

test('mem-bitfield-endian', mem_bitfield_endian_test)

gd_file_endian_test = executable(
    'gd-file-endian-test',
    sources: files('gd_FileEndianTests.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libgd, libos],
    native: true,
)

test('gd-file-endian', gd_file_endian_test)

os_host_mutex_test = executable(
    'os-host-mutex-test',
    sources: files('os_HostMutexTests.cpp'),
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

test('os-host-mutex', os_host_mutex_test, timeout: 10)

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
