PKG_CONFIG?=pkg-config
WAYLAND_PROTOCOLS!=$(PKG_CONFIG) --variable=pkgdatadir wayland-protocols
WAYLAND_SCANNER!=$(PKG_CONFIG) --variable=wayland_scanner wayland-scanner

PACKAGES="wlroots-0.18" wayland-server xkbcommon
CFLAGS_PKG_CONFIG!=$(PKG_CONFIG) --cflags $(PACKAGES)
CFLAGS+=$(CFLAGS_PKG_CONFIG)
LIBRARIES!=$(PKG_CONFIG) --libs $(PACKAGES)

COMPILER_FLAGS := -Wall -O3

INCLUDE_DIR := include
SOURCE_FILES := src/main.c src/filesystem.c src/keybinding.c src/commands.c src/log.c src/types/keybind_list.c src/types/keybind.c src/types/server.c src/types/output.c src/types/xdg_shell.c

OUT_DIR := build

all: clean build

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $(INCLUDE_DIR)/xdg-shell-protocol.h

build: xdg-shell-protocol.h
	if [ ! -d $(OUT_DIR) ]; then mkdir $(OUT_DIR); fi
	cc $(COMPILER_FLAGS) -o $(OUT_DIR)/EstrogenCompositor -I$(INCLUDE_DIR) $(SOURCE_FILES) -DWLR_USE_UNSTABLE $(LIBRARIES) $(CFLAGS)

clean:
	rm -rf $(OUT_DIR)/*

install:
	echo "WIP"

.PHONY: all build