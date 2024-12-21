#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "server.h"

struct e_xdg_shell
{
    struct wlr_xdg_shell* xdg_shell;
    struct e_server* server;

    struct wl_list toplevel_windows;

    //clients create new top level window
    struct wl_listener new_toplevel_window;

    //clients create new popup window
    struct wl_listener new_popup_window;

    struct wl_listener destroy;
};

struct e_xdg_shell* e_xdg_shell_create(struct e_server* server);