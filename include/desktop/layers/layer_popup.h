#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>

struct e_layer_popup
{
    struct wlr_layer_surface_v1* layer_surface_v1;

    struct wlr_xdg_popup* xdg_popup;

    struct wlr_scene_tree* scene_tree;

    struct wl_listener new_popup;

    //new surface state got committed
    struct wl_listener commit;
    //xdg_popup got destroyed
    struct wl_listener destroy;
};

//creates new layer popup
struct e_layer_popup* e_layer_popup_create(struct wlr_xdg_popup* xdg_popup, struct wlr_layer_surface_v1* layer_surface_v1);