#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>

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

    struct
    {
        //desktop background
        struct wlr_scene_tree* background;
        //tiled windows
        struct wlr_scene_tree* tiling;
        //floating windows
        struct wlr_scene_tree* floating; 
        //layer shell surfaces that display above everything, such as the rofi-wayland launcher
        struct wlr_scene_tree* overlay;
    } layers;
};

struct e_scene* e_scene_create(struct wl_display* display);

//get output at specified index, returns NULL if failed
struct e_output* e_scene_get_output(struct e_scene* scene, int index);

//adds the given output to the given scene and handles its layout for it
void e_scene_add_output(struct e_scene* scene, struct e_output* output);

void e_scene_destroy(struct e_scene* scene);