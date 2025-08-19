#pragma once

#ifndef E_XWAYLAND_SUPPORT
#error "Add -DE_XWAYLAND_SUPPORT to enable xwayland support"
#endif

#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_scene.h>

#include <wlr/xwayland.h>

#include "desktop/views/view.h"

struct e_server;

// Xwayland implementation of a view.
struct e_xwayland_view
{
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

    struct wl_listener request_fullscreen;
    struct wl_listener request_maximize;

    // Xwayland surface wants to be configured in a specific way.
    struct wl_listener request_configure;
    struct wl_listener request_move;
    struct wl_listener request_resize;

    struct wl_listener set_title;
    struct wl_listener set_class;

    // Xwayland surface is destroyed.
    struct wl_listener destroy;
};

// Xwayland surface that doesn't want to be managed as a view. (Override redirect = true)
// We should let these do their thing.
struct e_xwayland_unmanaged
{
    struct e_server* server;

    struct wlr_xwayland_surface* xwayland_surface;

    struct wlr_scene_tree* tree;

    // Xwayland surface wants to be configured in a specific way.
    struct wl_listener request_configure;
    struct wl_listener set_geometry;

    // Surface becomes valid.
    struct wl_listener associate;
    // Surface becomes invalid.
    struct wl_listener dissociate;

    // Surface is ready to be displayed.
    struct wl_listener map;
    // Surface no longer wants to be displayed.
    struct wl_listener unmap;
    
    // Xwayland surface is destroyed.
    struct wl_listener destroy;
};

// Update useable geometry not covered by panels, docks, etc. for xwayland.
void e_server_update_xwayland_workareas(struct e_server* server);

/* xwayland view functions */

// Creates new xwayland view.
// Returns NULL on fail.
struct e_xwayland_view* e_xwayland_view_create(struct e_server* server, struct wlr_xwayland_surface* xwayland_surface);

/* xwayland unmanaged functions */

// Creates new xwayland unmanaged surface for server.
// Returns NULL on fail.
struct e_xwayland_unmanaged* e_xwayland_unmanaged_create(struct e_server* server, struct wlr_xwayland_surface* xwayland_surface);
