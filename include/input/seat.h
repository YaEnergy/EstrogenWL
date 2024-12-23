#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>

#include "input/input_manager.h"

//collection & management of input devices: keyboard, mouse, ...
struct e_seat
{
    struct e_input_manager* input_manager;
    struct wlr_seat* wlr_seat;

    struct wl_list keyboards;

    //surface that currently has focus
    struct wlr_surface* focus_surface;

    //new input device found
    struct wl_listener new_input;

    struct wl_listener destroy;
};

struct e_seat* e_seat_create(struct e_input_manager* input_manager, const char* name);

void e_seat_set_focus(struct e_seat* seat, struct wlr_surface* surface);

//returns true if seat has full focus on this surface
//(all active devices have focus on this surface)
bool e_seat_has_focus(struct e_seat* seat, struct wlr_surface* surface);

void e_seat_clear_focus(struct e_seat* seat);

void e_seat_add_keyboard(struct e_seat* seat, struct wlr_input_device* input);