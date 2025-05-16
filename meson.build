project(
    'EstrogenWL',
    'c',
    version: '1.0.0',
    meson_version: '>=1.1',
    default_options: [
        'c_std=c11', #wlroots uses atleast c11
        'warning_level=2',
        'werror=true',
    ],
)

arguments = [
    '-D_POSIX_C_SOURCE=200809L',
    '-DWLR_USE_UNSTABLE',
    '-Wno-unused-parameter',
    '-Wmissing-include-dirs',
    '-Wpointer-arith'
]

estrogenwl_includedir = include_directories('include')

estrogenwl_src = files(
    'src/main.c',
    'src/server.c',
    'src/commands.c',
    'src/config.c',
    'src/session.c',
    
    'src/desktop/desktop.c',
    'src/desktop/output.c',
    'src/desktop/xdg_shell.c',
    'src/desktop/xdg_popup.c',

    'src/desktop/tree/workspace.c',
    'src/desktop/tree/container.c',
    'src/desktop/tree/node.c',

    'src/desktop/layer_shell/layer_shell.c',
    'src/desktop/layer_shell/layer_surface.c',

    'src/desktop/views/view.c',
    'src/desktop/views/toplevel_view.c',

    'src/input/seat.c',
    'src/input/keyboard.c',
    'src/input/keybind.c',
    'src/input/cursor.c',

    'src/util/filesystem.c',
    'src/util/list.c',
    'src/util/log.c',
)

cc = meson.get_compiler('c')

# main dependencies

wlroots = dependency(
    'wlroots-0.19',
    required: true,
    default_options: ['default_library=static', 'examples=false'],
    version: ['>=0.19', '<0.20.0'],
)

wayland_server = dependency('wayland-server', required: true)
wayland_cursor = dependency('wayland-cursor', required: true)
wayland_protocols = dependency('wayland-protocols', required: true)

xkbcommon = dependency('xkbcommon', required: true)

pixman = dependency('pixman-1', required: true)
libdrm = dependency('libdrm', required: true)
libudev = dependency('libudev', required: true)
libinput = dependency('libinput', required: true)

deps = [
    wlroots,
    wayland_server,
    wayland_cursor,
    wayland_protocols,
    xkbcommon,
    pixman,
    libdrm,
    libudev,
    libinput,
]

# xwayland support dependancies

xcb = dependency('xcb', required: get_option('xwayland'))

if xcb.found()
    message('xwayland is supported')

    arguments += '-DE_XWAYLAND_SUPPORT'
    deps += xcb

    estrogenwl_src += ['src/desktop/xwayland.c', 'src/desktop/views/xwayland_view.c', 'src/desktop/xwayland_unmanaged.c']
endif

# executable

subdir('protocols')

if (get_option('verbose'))
    arguments += '-DE_VERBOSE'
endif

add_project_arguments(
    arguments,
    language: 'c',
)

exe = executable('EstrogenWL', sources: estrogenwl_src + protocol_src, dependencies: [ deps ], include_directories: [ estrogenwl_includedir ])

test('basic', exe)

summary({
    'xwayland': xcb.found(),
    'verbose': get_option('verbose'),
})