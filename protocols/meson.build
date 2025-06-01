wayland_protocols_dir = wayland_protocols.get_variable('pkgdatadir')
wayland_scanner = find_program('wayland-scanner', required: true)

wayland_server_protocols = [
    wayland_protocols_dir / 'stable/xdg-shell/xdg-shell.xml',
    wayland_protocols_dir / 'stable/tablet/tablet-v2.xml',
    wayland_protocols_dir / 'staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml',
    wayland_protocols_dir / 'staging/ext-image-capture-source/ext-image-capture-source-v1.xml',
    wayland_protocols_dir / 'staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml',
    wayland_protocols_dir / 'staging/cursor-shape/cursor-shape-v1.xml',
    'cosmic-workspace-unstable-v1.xml',
    'wlr-layer-shell-unstable-v1.xml',
]

private_code_generator = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@']
)

server_header_generator = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.h',
    arguments: ['server-header', '@INPUT@', '@OUTPUT@']
)

protocol_src = []

foreach protocol_file : wayland_server_protocols
    protocol_src += private_code_generator.process(protocol_file)
    protocol_src += server_header_generator.process(protocol_file)
endforeach

