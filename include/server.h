#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>

#include "windows/xdg_shell.h"
#include "input/input_manager.h"

struct e_server
{
    //handles accepting clients from Unix socket, managing wl globals, ...
    struct wl_display* display;

    //handles input and output hardware
    struct wlr_backend* backend;
    struct wl_listener backend_destroy;
    
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

    //handles protocol for application windows
    struct e_xdg_shell* xdg_shell;

    struct wl_list outputs;
    struct wl_listener new_output;

    //collection & management of input devices: keyboard, mouse, ...
    struct e_input_manager* input_manager;
};

int e_server_init(struct e_server* server);

bool e_server_run(struct e_server* server);

void e_server_destroy(struct e_server* server);

//get output at specified index, returns NULL if failed
struct e_output* e_server_get_output(struct e_server* server, int index);