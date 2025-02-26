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
    install_dir: join_paths(get_option('prefix'))
)

# Install the man page using install_man
install_man('servicemaster.1')
