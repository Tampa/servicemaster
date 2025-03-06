project(
    'ServiceMaster',
    'c',
    version: '1.7.0',
    default_options: ['warning_level=3', 'buildtype=release', 'strip=true', 'c_std=c99'],
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
    'config.c',
    'lib/toml.c',
    dependencies: [ncurses_dep, systemd_dep],
    install: true,
    install_dir: get_option('bindir'),
)

# Install the man page to the correct location
install_data(
    'servicemaster.1',
    install_dir: join_paths(get_option('mandir'), 'man1')
)

# Install config file to /etc/servicemaster
config_dir = '/etc/servicemaster'
install_data(
    'servicemaster.toml',
    install_dir: config_dir,
)

meson.add_install_script('mkdir', '-p', config_dir)
