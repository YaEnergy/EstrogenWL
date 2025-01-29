# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.

PKG_CONFIG?=pkg-config
WAYLAND_PROTOCOLS!=$(PKG_CONFIG) --variable=pkgdatadir wayland-protocols
WAYLAND_SCANNER!=$(PKG_CONFIG) --variable=wayland_scanner wayland-scanner

PACKAGES="wlroots-0.18" wayland-server xkbcommon
CFLAGS_PKG_CONFIG!=$(PKG_CONFIG) --cflags $(PACKAGES)
CFLAGS+=$(CFLAGS_PKG_CONFIG)
LIBRARIES!=$(PKG_CONFIG) --libs $(PACKAGES)

# -Wno-unused-parameter is added to silence warnings for unused void* data args in wl listener notify callbacks,
# as many are not useful
COMPILER_FLAGS := -Wall -Wextra -Wno-unused-parameter -g -xc

INCLUDE_DIR := include
PROTOCOL_DIR := protocols
PROTOCOL_INCLUDE_DIR := protocols/include

SOURCE_UTIL_FILES := src/util/log.c src/util/filesystem.c

SOURCE_DESKTOP_LAYERS_FILES := src/desktop/layers/layer_shell.c src/desktop/layers/layer_surface.c src/desktop/layers/layer_popup.c
SOURCE_DESKTOP_WINDOWS_FILES := src/desktop/windows/xwayland_window.c src/desktop/windows/toplevel_window.c src/desktop/windows/window.c
SOURCE_DESKTOP_TREE_FILES := src/desktop/tree/node.c src/desktop/tree/container.c
SOURCE_DESKTOP_FILES := $(SOURCE_DESKTOP_LAYERS_FILES) $(SOURCE_DESKTOP_WINDOWS_FILES) $(SOURCE_DESKTOP_TREE_FILES) src/desktop/xwayland.c src/desktop/scene.c src/desktop/xdg_popup.c src/desktop/xdg_shell.c src/desktop/gamma_control_manager.c

SOURCE_INPUT_FILES := src/input/input_manager.c src/input/seat.c src/input/cursor.c src/input/keyboard.c src/input/keybind_list.c src/input/keybind.c src/input/keybinding.c

SOURCE_TOP_FILES := src/commands.c src/server.c src/output.c

SOURCE_FILES := src/main.c $(SOURCE_TOP_FILES) $(SOURCE_DESKTOP_FILES) $(SOURCE_INPUT_FILES) $(SOURCE_UTIL_FILES)

OUT_DIR := build

all: clean build

xdg-shell-protocol.h:
	if [ ! -d $(PROTOCOL_INCLUDE_DIR) ]; then mkdir $(PROTOCOL_INCLUDE_DIR); fi 
	
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $(PROTOCOL_INCLUDE_DIR)/xdg-shell-protocol.h

wlr-layer-shell-unstable-v1-protocol.h:
	if [ ! -d $(PROTOCOL_INCLUDE_DIR) ]; then mkdir $(PROTOCOL_INCLUDE_DIR); fi 

	$(WAYLAND_SCANNER) server-header \
		$(PROTOCOL_DIR)/wlr-layer-shell-unstable-v1.xml $(PROTOCOL_INCLUDE_DIR)/wlr-layer-shell-unstable-v1-protocol.h

gen-protocols: xdg-shell-protocol.h wlr-layer-shell-unstable-v1-protocol.h

build: gen-protocols
	if [ ! -d $(OUT_DIR) ]; then mkdir $(OUT_DIR); fi
	cc $(COMPILER_FLAGS) -o $(OUT_DIR)/EstrogenWL -I$(INCLUDE_DIR) -I$(PROTOCOL_INCLUDE_DIR) $(SOURCE_FILES) -DWLR_USE_UNSTABLE $(LIBRARIES) $(CFLAGS)

clean:
	rm -rf $(OUT_DIR)/*

install:
	echo "WIP"

.PHONY: all build