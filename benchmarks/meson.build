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
