wayland_protocols_dir = wayland_protocols.get_variable('pkgdatadir')
wayland_scanner = find_program('wayland-scanner')

wayland_server_protocols = [
    wayland_protocols_dir / 'stable/xdg-shell/xdg-shell.xml',
    'wlr-layer-shell-unstable-v1.xml'
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

