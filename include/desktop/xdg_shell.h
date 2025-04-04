#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>

struct e_desktop;

struct e_xdg_shell
{
    struct wlr_xdg_shell* xdg_shell;
    struct e_desktop* desktop;

    //clients create new top level window
    struct wl_listener new_toplevel_window;

    struct wl_listener destroy;
};

// Returns NULL on fail.
struct e_xdg_shell* e_xdg_shell_create(struct wl_display* display, struct e_desktop* desktop);