#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

struct e_popup_window
{
    struct wlr_xdg_popup* xdg_popup;

    struct wlr_scene_tree* scene_tree;

    //new surface state got committed
    struct wl_listener commit;
    //xdg_popup got destroyed
    struct wl_listener destroy;
};

//creates new popup window
struct e_popup_window* e_popup_window_create(struct wlr_xdg_popup* xdg_popup);