#pragma once

#include <wayland-server-core.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include "input/input_manager.h"

//display of cursor image and management cursor
struct e_cursor
{
    struct e_input_manager* input_manager;
    struct wlr_cursor* wlr_cursor;
    struct wlr_xcursor_manager* xcursor_manager;

    struct wl_listener frame;

    struct wl_listener button;

    struct wl_listener motion;
    struct wl_listener motion_absolute;

    //scrolling mouse wheel event
    struct wl_listener axis;
};

struct e_cursor* e_cursor_create(struct e_input_manager* input_manager, struct wlr_output_layout* output_layout);

void e_cursor_destroy(struct e_cursor* cursor);