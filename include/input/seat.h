#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_cursor_shape_v1.h>

#include "input/cursor.h"

struct e_view;

// Drag & drop action
struct e_dnd_action
{
    struct wlr_scene_tree* icon;

    struct wl_listener motion;
    struct wl_listener destroy;
};

// Collection & management of input devices: keyboard, mouse, ...
struct e_seat
{
    struct e_server* server;

    struct wlr_seat* wlr_seat;

    struct wl_list keyboards;

    struct e_cursor* cursor;

    // Tree used to store drag icons
    struct wlr_scene_tree* drag_icon_tree;

    // Current drag & drop action
    struct e_dnd_action current_dnd;

    // handles cursor shape protocol
    struct wlr_cursor_shape_manager_v1* cursor_shape_manager;

    // surface that currently has focus
    struct wlr_surface* focus_surface;
    struct wl_listener focus_surface_unmap;

    // client requests to set the surface of the cursor
    struct wl_listener request_set_cursor;

    // user requests to set selection (copying data)
    struct wl_listener request_set_selection;

    // user requests to set primary selection (selecting data)
    struct wl_listener request_set_primary_selection;

    struct wl_listener request_start_drag;
    struct wl_listener start_drag;

    struct wl_listener request_set_cursor_shape;

    struct wl_listener destroy;
};

// Returns NULL on fail.
struct e_seat* e_seat_create(struct e_server* server, struct wlr_output_layout* output_layout, const char* name);

// Add a new input device to a seat.
void e_seat_add_input_device(struct e_seat* seat, struct wlr_input_device* input);

// Set seat keyboard focus on a surface, doing nothing extra. (AKA raw focus)
// surface is allowed to be NULL.
// Returns whether this was succesful or not.
bool e_seat_set_focus_surface(struct e_seat* seat, struct wlr_surface* surface);

// Returns true if seat has focus on this surface.
bool e_seat_has_focus(struct e_seat* seat, struct wlr_surface* surface);

// Destroy seat.
void e_seat_destroy(struct e_seat* seat);
