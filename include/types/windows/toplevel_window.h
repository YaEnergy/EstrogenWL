#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_xdg_shell.h>

#include <wlr/types/wlr_scene.h>

#include "types/server.h"

struct e_toplevel_window
{
    struct e_server* server;

    struct wlr_xdg_toplevel* xdg_toplevel;
    
    struct wlr_scene_tree* scene_tree;

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;

    struct wl_list link;

    //TODO: request resize, fullscreen, ... events
};

//creates new top level window inside server
struct e_toplevel_window* e_toplevel_window_create(struct e_server* server, struct wlr_xdg_toplevel* xdg_toplevel);