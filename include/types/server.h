#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_cursor.h>

struct e_server
{
    struct wl_display* display;
    struct wlr_cursor* cursor;

    struct wlr_backend* backend;
    
    struct wlr_renderer* renderer;

    struct wl_list outputs;
    struct wl_listener new_output;
};