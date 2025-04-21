#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>

#include "views/view.h"

#define E_XDG_WM_BASE_VERSION 6

struct e_desktop;

struct e_xdg_shell
{
    struct wlr_xdg_shell* xdg_shell;
    struct e_desktop* desktop;

    //clients create new top level view
    struct wl_listener new_toplevel_view;

    struct wl_listener destroy;
};

struct e_toplevel_view
{
    //base view
    struct e_view base;

    struct wlr_xdg_toplevel* xdg_toplevel;

    int32_t scheduled_x, scheduled_y;

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

// Returns NULL on fail.
struct e_xdg_shell* e_xdg_shell_create(struct wl_display* display, struct e_desktop* desktop);

//creates new top level view on desktop
struct e_toplevel_view* e_toplevel_view_create(struct e_desktop* desktop, struct wlr_xdg_toplevel* xdg_toplevel);
