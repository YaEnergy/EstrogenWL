#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>

#include "desktop/xdg_shell.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "server.h"

struct e_layer_surface
{
    struct e_server* server;

    struct wlr_scene_layer_surface_v1* scene_layer_surface_v1;

    struct wlr_scene_tree* scene_tree;
    
    //new xdg popup
    struct wl_listener new_popup;

    //new surface state got committed
    struct wl_listener commit;

    struct wl_listener destroy;

    struct wl_list link;
};

struct e_layer_surface* e_layer_surface_create(struct e_server* server, struct wlr_layer_surface_v1* wlr_layer_surface_v1);

//configures an e_layer_surface's layout, updates remaining area
void e_layer_surface_configure(struct e_layer_surface* layer_surface, struct wlr_box* full_area, struct wlr_box* remaining_area);

//some helper getter functions

//returns this layer surface's layer
enum zwlr_layer_shell_v1_layer e_layer_surface_get_layer(struct e_layer_surface* layer_surface);

//returns this layer surface's wlr output
struct wlr_output* e_layer_surface_get_wlr_output(struct e_layer_surface* layer_surface);

struct e_layer_surface* e_layer_surface_from_surface(struct wlr_surface* surface);