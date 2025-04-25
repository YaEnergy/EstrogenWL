#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

#include "output.h"

struct e_config;
struct e_seat;
struct e_layer_surface;

//see: wlr-layer-shell-unstable-v1-protocol.h @ enum zwlr_layer_shell_v1_layer
struct e_layer_trees
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
    struct wlr_scene_tree* overlay;
    // Xwayland unmanaged surfaces.
    struct wlr_scene_tree* unmanaged;
};

// main struct handling what is visible to the user (root tree, outputs) & input (seat).
// basically for clients to make themselves visible and what views and layer surfaces need to communicate with.
struct e_desktop
{
    struct e_config* config;

    struct wl_display* display;

    struct wl_list outputs; //struct e_output* 
    // wlroots utility for working with arrangement of screens in a physical layout
    struct wlr_output_layout* output_layout;

    // root node of the scene.
    // handles all rendering & damage tracking, 
    // use this to add renderable things to the scene graph 
    // and then call wlr_scene_commit_output to render the frame
    struct wlr_scene* scene;
    // layout of outputs in the scene
    struct wlr_scene_output_layout* scene_layout;
    
    struct e_layer_trees layers;

    // nodes that are waiting to be reparented
    struct wlr_scene_tree* pending;

    struct wl_list views; //struct e_view*

    // collection & management of input devices: keyboard, mouse, ...
    struct e_seat* seat;
};

// Creates a desktop.
// Returns NULL on fail.
struct e_desktop* e_desktop_create(struct wl_display* display, struct wlr_compositor* compositor, struct e_config* config);

/* outputs */

//adds the given output to the given scene and handles its layout for it
void e_desktop_add_output(struct e_desktop* desktop, struct e_output* output);

//get output at specified index, returns NULL if failed
struct e_output* e_desktop_get_output(struct e_desktop* desktop, int index);

/* scene */

// Finds the scene surface at the specified layout coords in given scene graph.
// Also translates the layout coords to the surface coords if not NULL. (sx, sy)
// NULL for sx & sy is allowed.
// Returns NULL if nothing is found.
struct wlr_scene_surface* e_desktop_scene_surface_at(struct wlr_scene_node* node, double lx, double ly, double* sx, double* sy);

/* destruction */

void e_desktop_destroy(struct e_desktop* desktop);
