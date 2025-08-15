#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>

//TODO: comments
//TODO: finish header

struct e_view;

struct e_ext_foreign_toplevel
{
    struct e_view* view;

    //TODO: struct wlr_ext_foreign_toplevel_handle_v1* ext_handle;

    //TODO: struct wl_listener destroy;
};

struct e_wlr_foreign_toplevel
{
    struct e_view* view;

    struct wlr_foreign_toplevel_handle_v1* wlr_handle;

    //TODO: struct wl_listener request_maximize;
    //TODO: struct wl_listener request_minimize;
    //TODO: struct wl_listener request_activate;
    //TODO: struct wl_listener request_fullscreen;
    //TODO: struct wl_listener request_close;

    //TODO: struct wl_listener set_rectangle;

    //TODO: struct wl_listener destroy;
};

struct e_foreign_toplevel
{
    struct e_wlr_foreign_toplevel wlr;
    struct e_ext_foreign_toplevel ext;
};

// Returns NULL on fail.
struct e_foreign_toplevel* e_foreign_toplevel_create(struct e_view* view);

void e_foreign_toplevel_destroy(struct e_foreign_toplevel* foreign_toplevel);