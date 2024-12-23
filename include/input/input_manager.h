#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>

#include "server.h"

#include "input/keybind_list.h"
#include "input/seat.h"

struct e_input_manager
{
    struct e_server* server;

    struct e_seat* seat;

    struct e_cursor* cursor;

    struct e_keybind_list keybind_list;

    //new input found on the backend
    struct wl_listener new_input;
};

struct e_input_manager* e_input_manager_create(struct e_server* server);

void e_input_manager_destroy(struct e_input_manager* input_manager);