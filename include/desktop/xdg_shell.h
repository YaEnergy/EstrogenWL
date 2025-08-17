#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>

#include "views/view.h"

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
    struct wl_listener set_app_id;

    //xdg_toplevel got destroyed
    struct wl_listener destroy;
};

// Temporary surface for toplevel views.
struct e_xdg_popup
{
    struct e_view* view;

    struct wlr_xdg_popup* xdg_popup;
    struct wlr_scene_tree* tree;

    struct wl_listener reposition;
    struct wl_listener new_popup;
    struct wl_listener commit;
    struct wl_listener destroy;
};

// Creates new toplevel view.
// Returns NULL on fail.
struct e_toplevel_view* e_toplevel_view_create(struct e_server* server, struct wlr_xdg_toplevel* xdg_toplevel);
