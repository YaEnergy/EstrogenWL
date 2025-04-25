#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

// Temporary surface.
struct e_xdg_popup
{
    struct wlr_xdg_popup* xdg_popup;

    struct wlr_scene_tree* scene_tree;

    struct wl_listener new_popup;
    //TODO: struct wl_listener reposition;
    
    //new surface state got committed
    struct wl_listener commit;
    //xdg_popup got destroyed
    struct wl_listener destroy;
};

// Creates new xdg popup.
// Returns NULL on fail.
struct e_xdg_popup* e_xdg_popup_create(struct wlr_xdg_popup* xdg_popup, struct wlr_scene_tree* parent);
