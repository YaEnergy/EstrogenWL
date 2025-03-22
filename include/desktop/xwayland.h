#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>

#include <wlr/xwayland.h>

struct e_desktop;

//implementation xwayland shell v1 protocol, and X11 wm 
struct e_xwayland
{
    struct e_desktop* desktop;

    struct wlr_xwayland* wlr_xwayland;

    struct wl_listener new_surface;
};

struct e_xwayland* e_xwayland_create(struct e_desktop* desktop, struct wl_display* display, struct wlr_compositor* compositor, struct wlr_seat* seat, bool lazy);

void e_xwayland_destroy(struct e_xwayland* xwayland);