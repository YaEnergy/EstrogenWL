#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>

#include <wlr/xwayland.h>

#include <xcb/xproto.h>

#include "desktop/views/view.h"

enum e_xcb_atom_type
{
    E_NET_WM_WINDOW_TYPE_DESKTOP,
    E_NET_WM_WINDOW_TYPE_DOCK,
    E_NET_WM_WINDOW_TYPE_TOOLBAR,
    E_NET_WM_WINDOW_TYPE_MENU,
    E_NET_WM_WINDOW_TYPE_UTILITY,
    E_NET_WM_WINDOW_TYPE_SPLASH,
    E_NET_WM_WINDOW_TYPE_DIALOG,
    E_NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
    E_NET_WM_WINDOW_TYPE_POPUP_MENU,
    E_NET_WM_WINDOW_TYPE_TOOLTIP,
    E_NET_WM_WINDOW_TYPE_NOTIFICATION,
    E_NET_WM_WINDOW_TYPE_COMBO,
    E_NET_WM_WINDOW_TYPE_DND,
    E_NET_WM_WINDOW_TYPE_NORMAL,
    E_ATOMS_LEN //amount of atoms, not an atom
};

struct e_desktop;

// Implementation xwayland shell v1 protocol, and X11 wm.
struct e_xwayland
{
    struct e_desktop* desktop;

    struct wlr_xwayland* wlr_xwayland;

    xcb_atom_t atoms[E_ATOMS_LEN];

    struct wl_listener ready;
    struct wl_listener new_surface;
};

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

/* xwayland functions */

// Returns NULL on fail.
struct e_xwayland* e_xwayland_create(struct e_desktop* desktop, struct wl_display* display, struct wlr_compositor* compositor, struct wlr_seat* seat, bool lazy);

// Update useable geometry not covered by panels, docks, etc.
void e_xwayland_update_workarea(struct e_xwayland* xwayland);

void e_xwayland_destroy(struct e_xwayland* xwayland);

/* xwayland view functions */

//creates new xwayland view on desktop
struct e_xwayland_view* e_xwayland_view_create(struct e_desktop* desktop, struct e_xwayland* xwayland, struct wlr_xwayland_surface* xwayland_surface);