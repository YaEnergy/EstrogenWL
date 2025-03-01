#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>

#include "desktop/layers/layer_shell.h"
#include "desktop/xdg_shell.h"
#include "desktop/xwayland.h"
#include "input/seat.h"

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
    //renderer lost gpu
    struct wl_listener renderer_lost;

    //used by clients to create surfaces & regions
    struct wlr_compositor* compositor;

    //root scene tree for all layers, and further down windows and layer surfaces
    struct e_scene* scene;

    //handles protocol for application windows
    struct e_xdg_shell* xdg_shell;

    //handles protocol for layers
    struct e_layer_shell* layer_shell;

    struct wl_listener new_output;

    //collection & management of input devices: keyboard, mouse, ...
    struct e_seat* seat;

    //xwayland
    struct e_xwayland* xwayland;
};

int e_server_init(struct e_server* server);

bool e_server_run(struct e_server* server);

void e_server_fini(struct e_server* server);