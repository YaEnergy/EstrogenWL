#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

#include "desktop/tree/workspace.h"

struct e_desktop;

// Desktop output, possibly displaying a workspace.
struct e_output
{
    struct wl_list link; //e_desktop::outputs
    struct e_desktop* desktop;
    struct wlr_output* wlr_output;

    struct wlr_output_layout* layout;

    // Area tiled views are allowed to use.
    struct wlr_box usable_area;

    // Workspace that output is currently displaying, may be NULL.
    struct e_workspace* active_workspace;

    //output has a new frame ready
    struct wl_listener frame;

    //new state for X11 or wayland backend, must be committed
    struct wl_listener request_state;

    struct wl_listener destroy;

    struct wl_list layer_surfaces; //struct e_layer_surface*
};

struct e_output* e_output_create(struct e_desktop* desktop, struct wlr_output* wlr_output);

// Get topmost layer surface that requests exclusive focus.
// Returns NULL if none.
struct e_layer_surface* e_output_get_exclusive_topmost_layer_surface(struct e_output* output);

// Display given workspace.
// Given workspace must be inactive, but is allowed to be NULL.
bool e_output_display_workspace(struct e_output* output, struct e_workspace* workspace);

void e_output_arrange(struct e_output* output);
