#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_cursor.h>

struct e_server
{
    struct wl_display* display;
    struct wlr_cursor* cursor;

    struct wl_list outputs;
};