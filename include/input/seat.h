#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include "input/cursor.h"

struct e_view;
struct e_desktop;

// Collection & management of input devices: keyboard, mouse, ...
struct e_seat
{
    struct e_desktop* desktop;

    struct wlr_seat* wlr_seat;

    struct wl_list keyboards;

    struct e_cursor* cursor;

    // surface that currently has focus
    struct wlr_surface* focus_surface;
    struct wl_listener focus_surface_unmap;

    // surface that previously has focus
    struct wlr_surface* previous_focus_surface;

    // client requests to set the surface of the cursor
    struct wl_listener request_set_cursor;

    // user requests to set selection (copying data)
    struct wl_listener request_set_selection;

    // user requests to set primary selection (selecting data)
    struct wl_listener request_set_primary_selection;

    struct wl_listener request_start_drag;

    struct wl_listener destroy;
};

// Returns NULL on fail.
struct e_seat* e_seat_create(struct wl_display* display, struct e_desktop* desktop, struct wlr_output_layout* output_layout, const char* name);

// Add a new input device to a seat.
void e_seat_add_input_device(struct e_seat* seat, struct wlr_input_device* input);

// Set seat focus on a view if possible.
void e_seat_set_focus_view(struct e_seat* seat, struct e_view* view);

// Set seat focus on a layer surface if possible.
void e_seat_set_focus_layer_surface(struct e_seat* seat, struct wlr_layer_surface_v1* layer_surface);

// Gets the type of surface (view or layer surface) and sets seat focus.
// This will do nothing if surface isn't of a type that should be focused on by the seat.
void e_seat_set_focus_surface_type(struct e_seat* seat, struct wlr_surface* surface);

// Returns true if seat has focus on this surface.
bool e_seat_has_focus(struct e_seat* seat, struct wlr_surface* surface);

// Returns view currently in focus.
// Returns NULL if no view has focus.
struct e_view* e_seat_focused_view(struct e_seat* seat);

// Returns view previously in focus.
// Returns NULL if no view had focus.
struct e_view* e_seat_prev_focused_view(struct e_seat* seat);

// Returns true if seat has focus on a layer surface with exclusive interactivity.
bool e_seat_has_exclusive_layer_focus(struct e_seat* seat);

void e_seat_clear_focus(struct e_seat* seat);
