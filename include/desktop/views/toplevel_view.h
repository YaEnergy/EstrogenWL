#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

#include "desktop/views/view.h"

struct e_desktop;

struct e_toplevel_view
{
    //base view
    struct e_view base;

    struct wlr_xdg_toplevel* xdg_toplevel;

    //surface is ready to be displayed
    struct wl_listener map;
    //surface no longer wants to be displayed
    struct wl_listener unmap;
    //new surface state got committed
    struct wl_listener commit;

    struct wl_listener new_popup;

    struct wl_listener request_fullscreen;
    struct wl_listener request_maximize;
    struct wl_listener request_move;
    struct wl_listener request_resize;

    struct wl_listener set_title;

    //xdg_toplevel got destroyed
    struct wl_listener destroy;

    //TODO: request resize, fullscreen, ... events
};

//creates new top level view on desktop
struct e_toplevel_view* e_toplevel_view_create(struct e_desktop* desktop, struct wlr_xdg_toplevel* xdg_toplevel);