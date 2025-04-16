#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>

#include <wlr/xwayland.h>

#include "desktop/views/view.h"
#include "desktop/xwayland.h"

struct e_desktop;

struct e_xwayland_view
{
    // Xwayland server that created this view
    struct e_xwayland* xwayland;

    // Base view.
    struct e_view base;

    struct wlr_xwayland_surface* xwayland_surface;

    // Surface becomes valid.
    struct wl_listener associate;
    // Surface becomes invalid.
    struct wl_listener dissociate;

    // Surface is ready to be displayed.
    struct wl_listener map;
    // Surface no longer wants to be displayed.
    struct wl_listener unmap;
    // New surface state got committed.
    struct wl_listener commit;

    // Surface wants to be mapped.
    struct wl_listener map_request;
    struct wl_listener request_maximize;
    struct wl_listener request_configure;
    struct wl_listener request_move;
    struct wl_listener request_resize;

    struct wl_listener set_title;

    //xwayland_surface got destroyed
    struct wl_listener destroy;

    //TODO: request resize, fullscreen, ... events
};

//creates new xwayland view on desktop
struct e_xwayland_view* e_xwayland_view_create(struct e_desktop* desktop, struct e_xwayland* xwayland, struct wlr_xwayland_surface* xwayland_surface);