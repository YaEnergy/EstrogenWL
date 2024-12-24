#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

#include "server.h"

struct e_toplevel_window
{
    struct e_server* server;

    struct wlr_xdg_toplevel* xdg_toplevel;
    
    struct wlr_scene_tree* scene_tree;

    //surface is ready to be displayed
    struct wl_listener map;
    //surface no longer wants to be displayed
    struct wl_listener unmap;
    //new surface state got committed
    struct wl_listener commit;
    //xdg_toplevel got destroyed
    struct wl_listener destroy;

    struct wl_list link;

    //TODO: request resize, fullscreen, ... events
};

//creates new top level window inside server
struct e_toplevel_window* e_toplevel_window_create(struct e_server* server, struct wlr_xdg_toplevel* xdg_toplevel);

void e_toplevel_window_set_position(struct e_toplevel_window* window, int x, int y);

void e_toplevel_window_set_size(struct e_toplevel_window* window, int32_t x, int32_t y);

struct e_toplevel_window* e_toplevel_window_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy);