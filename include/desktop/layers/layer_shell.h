#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_layer_shell_v1.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

struct e_layer_shell
{
    struct e_server* server;

    struct wlr_layer_shell_v1* wlr_layer_shell_v1;

    struct wl_list layer_surfaces; //struct e_layer_surface

    //new wlr_layer_surface_v1 that has been configured by client to configure and commit
    struct wl_listener new_surface;

    struct wl_listener destroy;
};

struct e_layer_shell* e_layer_shell_create(struct e_server* server);

void e_layer_shell_arrange_layer(struct e_layer_shell* layer_shell, struct wlr_output* wlr_output, enum zwlr_layer_shell_v1_layer layer);

void e_layer_shell_arrange_all_layers(struct e_layer_shell* layer_shell, struct wlr_output* wlr_output);