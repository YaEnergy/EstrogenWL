#pragma once

#include <stdint.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include <wlr/util/box.h>
#include <wlr/util/edges.h>

#include "input/seat.h"

#include "desktop/views/view.h"

struct e_output;
struct e_container;

enum e_cursor_mode
{
    E_CURSOR_MODE_DEFAULT,
    E_CURSOR_MODE_RESIZE,
    E_CURSOR_MODE_MOVE
};

struct e_cursor_context
{
    // May be NULL.
    struct wlr_scene_surface* scene_surface;
    // Local position on surface.
    double sx, sy;
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

    //grabbed container
    struct e_container* grab_container;
    struct wl_listener grab_container_destroy;
    double grab_start_x;
    double grab_start_y;
    float grab_start_tile_percentage;
    struct wlr_box grab_start_cbox;
    uint32_t grab_edges; //bitmask enum wlr_edges 
};

// Returns NULL on fail.
struct e_cursor* e_cursor_create(struct e_seat* seat, struct wlr_output_layout* output_layout);

// Outs cursor's current hover context.
void e_cursor_get_context(const struct e_cursor* cursor, struct e_cursor_context* context);

void e_cursor_set_mode(struct e_cursor* cursor, enum e_cursor_mode mode);

// Lets go of a possibly grabbed view, & sets cursor mode to default.
void e_cursor_reset_mode(struct e_cursor* cursor);

// Starts grabbing a container under the resize mode, resizing along specified edges.
// edges is bitmask of enum wlr_edges.
void e_cursor_start_container_resize(struct e_cursor* cursor, struct e_container* container, uint32_t edges);

// Starts grabbing a container under the move mode.
void e_cursor_start_container_move(struct e_cursor* cursor, struct e_container* container);

// Sets seat focus to whatever surface is under cursor.
// If nothing is under cursor, doesn't change seat focus.
void e_cursor_set_focus_hover(struct e_cursor* cursor);

void e_cursor_destroy(struct e_cursor* cursor);
