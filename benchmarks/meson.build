gx_renderer_benchmark = executable(
    'gx-renderer-benchmark',
    sources: files('gx_RendererBenchmark.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libsim],
    dependencies: sdl2_dep,
    native: true,
)

benchmark(
    'gx-renderer',
    gx_renderer_benchmark,
    args: ['2000', '192'],
    timeout: 120,
)

benchmark(
    'gx-renderer-legacy',
    gx_renderer_benchmark,
    args: ['2000', '192', 'legacy'],
    timeout: 120,
)

benchmark(
    'gx-renderer-map-fail',
    gx_renderer_benchmark,
    args: ['2000', '192', 'map-fail'],
    timeout: 120,
)

gx_texture_snapshot_benchmark = executable(
    'gx-texture-snapshot-benchmark',
    sources: files('gx_TextureSnapshotBenchmark.cpp'),
    include_directories: libporpoise_includes,
    link_with: [libsim],
    native: true,
)

benchmark(
    'gx-texture-snapshot',
    gx_texture_snapshot_benchmark,
    args: ['64'],
    timeout: 120,
)
