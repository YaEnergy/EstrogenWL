#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/types/wlr_seat.h>

//collection & management of input devices: keyboard, mouse, ...
struct e_seat
{
    struct wlr_seat* wlr_seat;

    struct wl_list keyboards;
    
    //new input device found
    struct wl_listener new_input;

    struct wl_listener destroy;
};

struct e_seat* e_seat_create(struct wl_display* display, struct wlr_backend* backend, const char* name);