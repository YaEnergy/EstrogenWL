#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "output.h"

struct e_config;
struct e_seat;
struct e_layer_surface;

//see: wlr-layer-shell-unstable-v1-protocol.h @ enum zwlr_layer_shell_v1_layer
struct e_layer_trees
{
    //desktop background
    struct wlr_scene_tree* background;
    //shell surfaces unders windows
    struct wlr_scene_tree* bottom;
    //tiled windows
    struct wlr_scene_tree* tiling;
    //floating windows
    struct wlr_scene_tree* floating; 
    //shell surfaces above windows
    struct wlr_scene_tree* top;
    //layer shell surfaces that display above everything
    struct wlr_scene_tree* overlay;
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
    struct wl_list layer_surfaces; //struct e_layer_surface*

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

//returns a wlr_surface pointer at the specified layout coords, 
//also outs the surface's node, and translates the layout coords to the surface coords
struct wlr_surface* e_desktop_wlr_surface_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_scene_node** snode, double* sx, double* sy);

/* layers */

void e_desktop_arrange_layer(struct e_desktop* desktop, struct wlr_output* wlr_output, enum zwlr_layer_shell_v1_layer layer);

void e_desktop_arrange_all_layers(struct e_desktop* desktop, struct wlr_output* wlr_output);

//get topmost layer surface that requests exclusive focus, may be NULL
struct e_layer_surface* e_desktop_get_exclusive_topmost_layer_surface(struct e_desktop* desktop);

/* destruction */

void e_desktop_destroy(struct e_desktop* desktop);