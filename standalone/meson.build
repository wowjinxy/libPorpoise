libporpoise_link_args = []

eppc_lcf = meson.project_source_root() / 'include' / 'dolphin' / 'eppc.lcf'

if(build_target == 'gc')
    libporpoise_link_args += [
        '-lcf',
        eppc_lcf
    ]
endif

libporpoise_exe = executable(
    'libporpoise',
    sources: files('main.cpp'),
    dependencies: [ libporpoise_dep ],
    link_args: [ libporpoise_link_args ],
    native: build_target != 'gc',
)