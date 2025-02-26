project(
    'ServiceMaster',
    'c',
    version: '1.5.0',
    default_options: ['warning_level=3', 'buildtype=release', 'strip=true', 'c_std=c11'],
)

ncurses_dep = dependency('ncurses')
systemd_dep = dependency('libsystemd')

executable(
    'servicemaster',
    'servicemaster.c',
    'sm_err.c',
    'bus.c',
    'display.c',
    'service.c',
    dependencies: [ncurses_dep, systemd_dep],
    install: true,
    install_dir: get_option('bindir'),
)

# Install the man page to the correct location
install_data(
    'servicemaster.1',
    install_dir: join_paths(get_option('mandir'), 'man1')
)
