#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

#include "desktop/windows/window.h"

#include "server.h"

struct e_toplevel_window
{
    //base window
    struct e_window* base;

    struct wlr_xdg_toplevel* xdg_toplevel;

    //surface is ready to be displayed
    struct wl_listener map;
    //surface no longer wants to be displayed
    struct wl_listener unmap;
    //new surface state got committed
    struct wl_listener commit;
    //xdg_toplevel got destroyed
    struct wl_listener destroy;

    //TODO: request resize, fullscreen, ... events
};

//creates new top level window inside server
struct e_toplevel_window* e_toplevel_window_create(struct e_server* server, struct wlr_xdg_toplevel* xdg_toplevel);