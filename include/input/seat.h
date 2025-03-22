#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>

#include "input/cursor.h"

struct e_desktop;

//collection & management of input devices: keyboard, mouse, ...
struct e_seat
{
    struct e_desktop* desktop;

    struct wlr_seat* wlr_seat;

    struct wl_list keyboards;

    struct e_cursor* cursor;

    //surface that currently has focus
    struct wlr_surface* focus_surface;
    //surface that previously has focus
    struct wlr_surface* previous_focus_surface;

    //client requests to set the surface of the cursor
    struct wl_listener request_set_cursor;

    //user requests to set selection (copying data)
    struct wl_listener request_set_selection;

    struct wl_listener destroy;
};

struct e_seat* e_seat_create(struct wl_display* display, struct e_desktop* desktop, struct wlr_output_layout* output_layout, const char* name);

void e_seat_add_input_device(struct e_seat* seat, struct wlr_input_device* input);

// only sets keyboard (if active) focus, pointer focus is only on hover
void e_seat_set_focus(struct e_seat* seat, struct wlr_surface* surface, bool override_exclusive);

//returns true if seat has focus on this surface
bool e_seat_has_focus(struct e_seat* seat, struct wlr_surface* surface);

//returns true if seat has focus on a layer surface with exclusive interactivity
bool e_seat_has_exclusive_layer_focus(struct e_seat* seat);

void e_seat_clear_focus(struct e_seat* seat);