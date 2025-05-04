#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include <wlr/util/box.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "output.h"

#define E_LAYER_SHELL_VERSION 4

struct e_desktop;

// Handles protocol for arranging clients in layers.
struct e_layer_shell
{
    struct e_desktop* desktop;

    struct wlr_layer_shell_v1* wlr_layer_shell_v1;

    // New wlr_layer_surface_v1.
    struct wl_listener new_surface;

    struct wl_listener destroy;
};

// Surfaces meant to be arranged in layers.
struct e_layer_surface
{
    struct e_desktop* desktop;

    struct e_output* output;

    struct wlr_scene_layer_surface_v1* scene_layer_surface_v1;

    // New surface state got committed.
    struct wl_listener commit;
    // Surface is ready to be displayed.
    struct wl_listener map;
    // Surface no longer wants to be displayed.
    struct wl_listener unmap;

    // New layer popup.
    struct wl_listener new_popup;
    // Layer surface got destroyed.
    struct wl_listener destroy;

    struct wl_list link; //e_desktop::layer_surfaces
};

/* layer shell functions */

// Create a layer shell.
// Returns NULL on fail.
struct e_layer_shell* e_layer_shell_create(struct wl_display* display, struct e_desktop* desktop);

/* layer surface functions */

// Create a layer surface for the given desktop.
// wlr_layer_surface_v1's output should not be NULL.
// Returns NULL on fail.
struct e_layer_surface* e_layer_surface_create(struct e_desktop* desktop, struct wlr_layer_surface_v1* wlr_layer_surface_v1);

// Configures an e_layer_surface's layout, updates remaining area.
void e_layer_surface_configure(struct e_layer_surface* layer_surface, struct wlr_box* full_area, struct wlr_box* remaining_area);

/* some helper getter functions for layer surfaces */

// Returns this layer surface's layer.
enum zwlr_layer_shell_v1_layer e_layer_surface_get_layer(struct e_layer_surface* layer_surface);

// Returns this layer surface's wlr output.
struct wlr_output* e_layer_surface_get_wlr_output(struct e_layer_surface* layer_surface);
