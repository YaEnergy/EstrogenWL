#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>

struct e_server
{
    struct wl_display* display;
    struct wlr_cursor* cursor;

    struct wlr_backend* backend;
    
    //allocates memory for pixel buffers 
    struct wlr_allocator* allocator;
    //renderer handles rendering, used by scene
    struct wlr_renderer* renderer;

    //wlroots utility for working with arrangement of screens in a physical layout
    struct wlr_output_layout* output_layout;

    //handles all rendering & damage tracking, 
    //use this to add renderable things to the scene graph 
    //and then call wlr_scene_commit_output to render the frame
    struct wlr_scene* scene;
    struct wlr_scene_output_layout* scene_layout;

    struct wl_list outputs;
    struct wl_listener new_output;
};

int e_server_init(struct e_server* server);