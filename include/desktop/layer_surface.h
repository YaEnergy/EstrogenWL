#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include "server.h"

struct e_layer_surface
{
    struct e_server* server;

    struct wlr_scene_layer_surface_v1* wlr_scene_layer_surface_v1;

    struct wlr_scene_tree* scene_tree;
    
    //TODO: implement layer surface popups
    //struct wl_listener new_popup;

    //new surface state got committed
    struct wl_listener commit;

    struct wl_listener destroy;
};

struct e_layer_surface* e_layer_surface_create(struct e_server* server, struct wlr_layer_surface_v1* wlr_layer_surface_v1);