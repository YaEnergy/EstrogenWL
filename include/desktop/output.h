#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

#include "desktop/tree/workspace.h"

struct e_server;
struct e_desktop;

//see: wlr-layer-shell-unstable-v1-protocol.h @ enum zwlr_layer_shell_v1_layer
struct e_output_desktop_layers
{
    //desktop background
    struct wlr_scene_tree* background;
    //layer shell surfaces unders views
    struct wlr_scene_tree* bottom;
    //tiled views
    struct wlr_scene_tree* tiling;
    //floating views
    struct wlr_scene_tree* floating; 
    //layer shell surfaces above views
    struct wlr_scene_tree* top;
    //layer shell surfaces that display above everything
    //or fullscreen surfaces
    struct wlr_scene_tree* overlay;
};

// Server compositor output region, usually a monitor, here always for a desktop. (Desktop output)
struct e_output
{
    struct e_server* server;
    
    struct wlr_output* wlr_output;

    struct wlr_output_layout* layout;
    struct wlr_scene_output* scene_output;

    // Area tiled views are allowed to use.
    struct wlr_box usable_area;

    struct e_list workspaces; //struct e_workspace*
    
    // Workspace that output is currently displaying, may be NULL.
    struct e_workspace* active_workspace;

    struct wlr_scene_tree* tree;
    struct e_output_desktop_layers layers;

    struct wl_list layer_surfaces; //struct e_layer_surface*

    // Output has a new frame ready
    struct wl_listener frame;

    // New state from X11 or wayland output backends to commit
    struct wl_listener request_state;

    struct wl_listener destroy;

    struct wl_list link; //e_desktop::outputs
};

// Returns NULL on fail.
struct e_output* e_output_create(struct wlr_output* output);

// Get topmost layer surface that requests exclusive focus.
// Returns NULL if none.
struct e_layer_surface* e_output_get_exclusive_topmost_layer_surface(struct e_output* output);

// Display given workspace.
// Given workspace must be inactive, but is allowed to be NULL.
bool e_output_display_workspace(struct e_output* output, struct e_workspace* workspace);

void e_output_arrange(struct e_output* output);

// Destroy the output.
void e_output_destroy(struct e_output* output);
