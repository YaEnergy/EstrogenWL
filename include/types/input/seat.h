#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/types/wlr_seat.h>

#include "types/input/keybind_list.h"
#include "types/server.h"

//collection & management of input devices: keyboard, mouse, ...
struct e_seat
{
    struct e_server* server;
    struct wlr_seat* wlr_seat;

    struct wl_list keyboards;

    struct e_keybind_list keybind_list;
    
    //new input device found
    struct wl_listener new_input;

    struct wl_listener destroy;
};

struct e_seat* e_seat_create(struct e_server* server, const char* name);