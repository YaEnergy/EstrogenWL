#pragma once

#include <wayland-server-core.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include <wlr/util/box.h>

#include "input/input_manager.h"

#include "desktop/windows/window.h"

enum e_cursor_mode
{
    E_CURSOR_MODE_DEFAULT,
    E_CURSOR_MODE_MOVE,
    E_CURSOR_MODE_RESIZE
};

//display of cursor image and management cursor
struct e_cursor
{
    struct e_input_manager* input_manager;
    struct wlr_cursor* wlr_cursor;
    struct wlr_xcursor_manager* xcursor_manager;

    enum e_cursor_mode mode;

    struct wl_listener frame;

    struct wl_listener button;

    struct wl_listener motion;
    struct wl_listener motion_absolute;

    //scrolling mouse wheel event
    struct wl_listener axis;

    //grabbed window
    struct e_window* grab_window;
    double grab_wx;
    double grab_wy;
    struct wlr_box grab_start_wbox;
};

struct e_cursor* e_cursor_create(struct e_input_manager* input_manager, struct wlr_output_layout* output_layout);

void e_cursor_set_mode(struct e_cursor* cursor, enum e_cursor_mode mode);
void e_cursor_reset_mode(struct e_cursor* cursor);

//start grabbing a window under the given mode, (should be RESIZE or MOVE)
void e_cursor_start_grab_window_mode(struct e_cursor* cursor, struct e_window* window, enum e_cursor_mode mode);

void e_cursor_destroy(struct e_cursor* cursor);