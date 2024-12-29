#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_compositor.h>

#include "output.h"

////root scene tree for all layers, and further down windows and layer surfaces, and handling the layout for it
struct e_scene
{
    //handles all rendering & damage tracking, 
    //use this to add renderable things to the scene graph 
    //and then call wlr_scene_commit_output to render the frame
    struct wlr_scene* wlr_scene;

    struct wlr_scene_output_layout* scene_layout;

    //wlroots utility for working with arrangement of screens in a physical layout
    struct wlr_output_layout* output_layout;

    struct wl_list outputs;

    //see: wlr-layer-shell-unstable-v1-protocol.h @ enum zwlr_layer_shell_v1_layer
    struct
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
    } layers;
};

struct e_scene* e_scene_create(struct wl_display* display);

//get output at specified index, returns NULL if failed
struct e_output* e_scene_get_output(struct e_scene* scene, int index);

//get output from given wlr_output, returns NULL if failed
struct e_output* e_scene_output_from_wlr(struct e_scene* scene, struct wlr_output* wlr_output);

//returns a wlr_surface pointer at the specified layout coords, 
//also outs the surface's node, and translates the layout coords to the surface coords
struct wlr_surface* e_scene_wlr_surface_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_scene_node** snode, double* sx, double* sy);

//adds the given output to the given scene and handles its layout for it
void e_scene_add_output(struct e_scene* scene, struct e_output* output);

void e_scene_destroy(struct e_scene* scene);