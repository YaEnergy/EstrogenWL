#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>

#include "config.h"

struct e_desktop;
struct e_seat;

struct wlr_xdg_shell;
struct wlr_layer_shell_v1;
#if E_XWAYLAND_SUPPORT
struct wlr_xwayland;
#endif

struct e_cosmic_workspace_manager;

#define E_COMPOSITOR_VERSION 6

#define E_XDG_WM_BASE_VERSION 6
#define E_LAYER_SHELL_VERSION 4

#define E_PRESENTATION_TIME_VERSION 2
#define E_EXT_DATA_CONTROL_V1_VERSION 1
#define E_EXT_IMAGE_CAPTURE_SOURCE_VERSION 1
#define E_EXT_IMAGE_COPY_CAPTURE_VERSION 1

#define E_LINUX_DRM_SYNCOBJ_VERSION 1

#define E_COSMIC_WORKSPACE_VERSION 2

// main struct handling:
//  - backend (wl_display, wlr_backend, wlr_allocator, wlr_renderer, shells, protocols ...)
//  - frontend (e_desktop)
//  - config
struct e_server
{
    struct e_config* config;

    struct wl_event_loop* event_loop;

    //event sources
    struct
    {
        struct wl_event_source* sigint;
        struct wl_event_source* sigterm;
    } sources;

    // handles accepting clients from Unix socket, managing wl globals, ...
    struct wl_display* display;

    // handles input and output hardware
    struct wlr_backend* backend;
    struct wl_listener new_input;
    struct wl_listener new_output;
    
    // allocates memory for pixel buffers 
    struct wlr_allocator* allocator;
    
    // renderer handles rendering, used by scene
    struct wlr_renderer* renderer;
    // renderer lost gpu
    struct wl_listener renderer_lost;

    // used by clients to create surfaces & regions
    struct wlr_compositor* compositor;

    // handles xdg shell protocol for xdg application views
    struct wlr_xdg_shell* xdg_shell;
    struct wl_listener new_toplevel;
    
    // handles wlr layer shell protocol for layer surfaces
    struct wlr_layer_shell_v1* layer_shell;
    struct wl_listener new_layer_surface;

#if E_XWAYLAND_SUPPORT
    // handles xwayland protocol, server and wm for xwayland application views
    struct wlr_xwayland* xwayland;
    // XCB connection is valid.
    struct wl_listener xwayland_ready;
    struct wl_listener new_xwayland_surface;
#endif

    struct e_cosmic_workspace_manager* cosmic_workspace_manager;

    // what the user interacts with
    struct e_desktop* desktop;

    // collection & management of input devices: keyboard, mouse, ...
    struct e_seat* seat;
};

// Init server output handling.
bool e_server_init_outputs(struct e_server* server);
void e_server_fini_outputs(struct e_server* server);

// Init xdg shell handling.
bool e_server_init_xdg_shell(struct e_server* server);
void e_server_fini_xdg_shell(struct e_server* server);

// Init layer shell handling.
bool e_server_init_layer_shell(struct e_server* server);
void e_server_fini_layer_shell(struct e_server* server);

#if E_XWAYLAND_SUPPORT
// Init xwayland.
bool e_server_init_xwayland(struct e_server* server, struct e_seat* seat, bool lazy);
void e_server_fini_xwayland(struct e_server* server);
#endif

int e_server_init(struct e_server* server, struct e_config* config);

bool e_server_start(struct e_server* server);

void e_server_run(struct e_server* server);

void e_server_terminate(struct e_server* server);

void e_server_fini(struct e_server* server);