project(
    'EstrogenWL',
    'c',
    default_options: [
        'warning_level=2',
        'werror=true',
    ],
)

add_project_arguments(
    [
        '-DWLR_USE_UNSTABLE',
        '-Wno-unused-parameter',
    ],
    language: 'c',
)

cc = meson.get_compiler('c')

wlroots = dependency('wlroots-0.18')

wayland_server = dependency('wayland-server')
wayland_cursor = dependency('wayland-cursor')
wayland_protocols = dependency('wayland-protocols')

xkbcommon = dependency('xkbcommon')

pixman = dependency('pixman-1')
libdrm = dependency('libdrm')
libudev = dependency('libudev')
libinput = dependency('libinput')

xcb = dependency('xcb')

deps = [
    wlroots,
    wayland_server,
    wayland_cursor,
    wayland_protocols,
    xkbcommon,
    pixman,
    libdrm,
    xcb,
    libudev,
    libinput,
]

estrogenwl_includedir = include_directories('include')

subdir('protocols')

estrogenwl_src = files(
    'src/main.c',
    'src/server.c',
    'src/output.c',
    'src/commands.c',
    'src/config.c',
    
    'src/desktop/desktop.c',
    'src/desktop/xdg_shell.c',
    'src/desktop/xdg_popup.c',
    'src/desktop/xwayland.c',
    'src/desktop/gamma_control_manager.c',

    'src/desktop/tree/container.c',
    'src/desktop/tree/node.c',

    'src/desktop/layers/layer_shell.c',
    'src/desktop/layers/layer_surface.c',
    'src/desktop/layers/layer_popup.c',

    'src/desktop/windows/window.c',
    'src/desktop/windows/toplevel_window.c',
    'src/desktop/windows/xwayland_window.c',

    'src/input/seat.c',
    'src/input/keyboard.c',
    'src/input/keybinding.c',
    'src/input/keybind.c',
    'src/input/cursor.c',

    'src/util/filesystem.c',
    'src/util/list.c',
    'src/util/log.c',
)

exe = executable('EstrogenWL', sources: estrogenwl_src + protocol_src, dependencies: [ deps ], include_directories: [ estrogenwl_includedir ])

test('basic', exe)