#pragma once

#include <wayland-server-core.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include <wlr/util/box.h>
#include <wlr/util/edges.h>

#include "input/seat.h"

#include "desktop/views/view.h"

struct e_output;

enum e_cursor_mode
{
    E_CURSOR_MODE_DEFAULT,
    E_CURSOR_MODE_RESIZE,
    E_CURSOR_MODE_MOVE
};

//display of cursor image and management cursor
struct e_cursor
{
    struct e_seat* seat;

    struct wlr_cursor* wlr_cursor;
    struct wlr_xcursor_manager* xcursor_manager;

    enum e_cursor_mode mode;

    struct wl_listener frame;

    struct wl_listener button;

    struct wl_listener motion;
    struct wl_listener motion_absolute;

    //scrolling mouse wheel event
    struct wl_listener axis;

    //grabbed view
    struct e_view* grab_view;
    struct wl_listener grab_view_unmap;
    double grab_start_x;
    double grab_start_y;
    float grab_start_tile_percentage;
    struct wlr_box grab_start_vbox;
    enum wlr_edges grab_edges;
};

// Returns NULL on fail.
struct e_cursor* e_cursor_create(struct e_seat* seat, struct wlr_output_layout* output_layout);

void e_cursor_set_mode(struct e_cursor* cursor, enum e_cursor_mode mode);

// Lets go of a possibly grabbed view, & sets cursor mode to default.
void e_cursor_reset_mode(struct e_cursor* cursor);

// Starts grabbing a view under the resize mode, resizing along specified edges/
void e_cursor_start_view_resize(struct e_cursor* cursor, struct e_view* view, enum wlr_edges edges);

// Starts grabbing a view under the move mode.
void e_cursor_start_view_move(struct e_cursor* cursor, struct e_view* view);

// Sets seat focus to whatever surface is under cursor.
// If nothing is under cursor, doesn't change seat focus.
void e_cursor_set_focus_hover(struct e_cursor* cursor);

void e_cursor_destroy(struct e_cursor* cursor);
