#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_layer_shell_v1.h>

struct e_desktop;

struct e_layer_shell
{
    struct e_desktop* desktop;

    struct wlr_layer_shell_v1* wlr_layer_shell_v1;

    //new wlr_layer_surface_v1 that has been configured by client to configure and commit
    struct wl_listener new_surface;

    struct wl_listener destroy;
};

struct e_layer_shell* e_layer_shell_create(struct wl_display* display, struct e_desktop* desktop);