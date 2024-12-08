#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include "server.h"

struct e_output
{
    struct wl_list link;
    struct e_server* server;
    struct wlr_output* wlr_output;

    struct wl_listener frame;

    struct wl_listener destroy;
};

//called when output is ready to display frame
void e_output_frame(struct wl_listener* listener, void* data);

void e_output_destroy(struct wl_listener* listener, void* data);