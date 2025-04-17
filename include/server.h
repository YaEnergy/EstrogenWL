#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>

#include "desktop/desktop.h"
#include "desktop/layer_shell.h"
#include "desktop/xdg_shell.h"
#include "desktop/xwayland.h"

#include "config.h"

// main struct handling:
//  - backend (wl_display, wlr_backend, wlr_allocator, wlr_renderer, shells, protocols ...)
//  - frontend (e_desktop)
//  - config
struct e_server
{
    struct e_config config;

    // handles accepting clients from Unix socket, managing wl globals, ...
    struct wl_display* display;

    // handles input and output hardware
    struct wlr_backend* backend;
    struct wl_listener new_input;
    struct wl_listener new_output;
    struct wl_listener backend_destroy;
    
    // allocates memory for pixel buffers 
    struct wlr_allocator* allocator;
    
    // renderer handles rendering, used by scene
    struct wlr_renderer* renderer;
    // renderer lost gpu
    struct wl_listener renderer_lost;

    // used by clients to create surfaces & regions
    struct wlr_compositor* compositor;

    // handles xdg shell protocol for xdg application views
    struct e_xdg_shell* xdg_shell;
    // handles wlr layer shell protocol for layer surfaces
    struct e_layer_shell* layer_shell;
    // handles xwayland protocol, server and wm for xwayland application views
    struct e_xwayland* xwayland;

    // what the user interacts with
    struct e_desktop* desktop;
};

int e_server_init(struct e_server* server);

bool e_server_run(struct e_server* server);

void e_server_fini(struct e_server* server);