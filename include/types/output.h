#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include "server.h"

//TODO: implement

struct e_output
{
    struct wl_list link;
    struct e_server* server;
    struct wlr_output* wlr_output;

    //TODO: add
    //struct wl_listener frame;

    struct wl_listener destroy;
};

void e_output_destroy(struct wl_listener* listener, void* data);