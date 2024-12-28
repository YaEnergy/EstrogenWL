#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include "server.h"

struct e_output
{
    struct wl_list link;
    struct e_server* server;
    struct wlr_output* wlr_output;

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

    //output has a new frame ready
    struct wl_listener frame;

    //new state for X11 or wayland backend, must be committed
    struct wl_listener request_state;

    struct wl_listener destroy;
};

struct e_output* e_output_create(struct e_server* server, struct wlr_output* wlr_output);