#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>

#include <wlr/xwayland.h>

#include "desktop/windows/window.h"

#include "server.h"

struct e_xwayland_window
{
    //base window
    struct e_window* base;

    struct wlr_xwayland_surface* xwayland_surface;

    //surface becomes valid
    struct wl_listener associate;
    //surface becomes invalid
    struct wl_listener dissociate;

    //surface is ready to be displayed
    struct wl_listener map;
    //surface no longer wants to be displayed
    struct wl_listener unmap;
    //new surface state got committed
    struct wl_listener commit;

    struct wl_listener request_maximize;
    struct wl_listener request_configure;
    struct wl_listener request_move;
    struct wl_listener request_resize;

    //xwayland_surface got destroyed
    struct wl_listener destroy;

    //TODO: request resize, fullscreen, ... events
};

//creates new xwayland window inside server
struct e_xwayland_window* e_xwayland_window_create(struct e_server* server, struct wlr_xwayland_surface* xwayland_surface);