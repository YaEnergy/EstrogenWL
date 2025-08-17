#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>

struct e_output;

struct e_view;

struct e_ext_foreign_toplevel_state
{
    // May be NULL.
    const char* title;
    // May be NULL.
    const char* app_id;
};

struct e_ext_foreign_toplevel
{
    struct e_view* view;

    struct e_ext_foreign_toplevel_state state;
    
    struct wlr_ext_foreign_toplevel_handle_v1* handle;

    struct wl_listener destroy;
};

struct e_wlr_foreign_toplevel
{
    struct e_view* view;

    struct wlr_foreign_toplevel_handle_v1* handle;

    //TODO: struct wl_listener request_maximize;
    //TODO: struct wl_listener request_minimize;
    struct wl_listener request_activate;
    struct wl_listener request_fullscreen;
    struct wl_listener request_close;

    //TODO: struct wl_listener set_rectangle;

    struct wl_listener destroy;
};

struct e_foreign_toplevel
{
    struct e_wlr_foreign_toplevel wlr;
    struct e_ext_foreign_toplevel ext;
};

// Should be done on view map.
// Returns NULL on fail.
struct e_foreign_toplevel* e_foreign_toplevel_create(struct e_view* view);

void e_foreign_toplevel_output_enter(struct e_foreign_toplevel* foreign_toplevel, struct e_output* output);
void e_foreign_toplevel_output_leave(struct e_foreign_toplevel* foreign_toplevel, struct e_output* output);

//TODO: set_parent, set_maximized, set_minimized

// title may be NULL.
void e_foreign_toplevel_set_title(struct e_foreign_toplevel* foreign_toplevel, const char* title);
// app_id may be NULL.
void e_foreign_toplevel_set_app_id(struct e_foreign_toplevel* foreign_toplevel, const char* app_id);
void e_foreign_toplevel_set_activated(struct e_foreign_toplevel* foreign_toplevel, bool activated);
void e_foreign_toplevel_set_fullscreen(struct e_foreign_toplevel* foreign_toplevel, bool fullscreen);

// Should be done on view unmap.
void e_foreign_toplevel_destroy(struct e_foreign_toplevel* foreign_toplevel);
