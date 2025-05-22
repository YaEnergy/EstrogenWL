#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>

#include "desktop/xdg_shell.h"

//TODO: comments
//TODO: finish header

struct e_foreign_toplevel_manager
{
    struct wlr_foreign_toplevel_manager_v1* wlr_manager;
    struct wlr_ext_foreign_toplevel_list_v1* ext_list;

    struct wl_list foreign_toplevels; //struct e_foreign_toplevel*
};

struct e_foreign_toplevel
{
    struct e_toplevel_view* toplevel_view;
    
    struct wlr_foreign_toplevel_handle_v1* wlr_handle;
    struct wlr_ext_foreign_toplevel_handle_v1* ext_handle;

    struct wl_listener request_maximize;
    struct wl_listener request_minimize;
    struct wl_listener request_activate;
    struct wl_listener request_fullscreen;
    struct wl_listener request_close;

    struct wl_listener set_rectangle;
    struct wl_listener destroy;

    struct wl_list link; //e_foreign_toplevel_manager::foreign_toplevels
};

bool e_foreign_toplevel_management_init(struct wl_display* display);

struct e_foreign_toplevel* e_foreign_toplevel_create(struct e_foreign_toplevel_manager* manager, struct e_toplevel_view* toplevel_view);

void e_foreign_toplevel_destroy(struct e_foreign_toplevel* foreign_toplevel);