#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>

#include <wlr/xwayland.h>

#include <xcb/xproto.h>

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

// Returns NULL on fail.
struct e_xwayland* e_xwayland_create(struct e_desktop* desktop, struct wl_display* display, struct wlr_compositor* compositor, struct wlr_seat* seat, bool lazy);

void e_xwayland_destroy(struct e_xwayland* xwayland);