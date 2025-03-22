#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

#include "desktop/tree/container.h"

struct e_desktop;

struct e_output
{
    struct wl_list link; //e_desktop::outputs
    struct e_desktop* desktop;
    struct wlr_output* wlr_output;

    struct wlr_output_layout* layout;

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

    //container for tiled containers
    struct e_container* root_tiling_container;

    //container for floating containers
    struct e_container* root_floating_container;

    //output has a new frame ready
    struct wl_listener frame;

    //new state for X11 or wayland backend, must be committed
    struct wl_listener request_state;

    struct wl_listener destroy;
};

struct e_output* e_output_create(struct e_desktop* desktop, struct wlr_output* wlr_output);

void e_output_arrange(struct e_output* output);