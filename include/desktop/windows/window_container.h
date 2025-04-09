#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <wayland-server-core.h>

#include <wayland-util.h>
#include <wlr/types/wlr_scene.h>

#include "desktop/tree/container.h"
#include "desktop/windows/window.h"

// Container that serves as decoration around a window.
struct e_window_container
{
    struct e_desktop* desktop;
    
    char* title;
    //struct wlr_scene_rect* border;

    bool tiled;
    
    struct e_window* view;

    struct e_container base;

    struct wl_listener view_set_tiled;
    struct wl_listener view_set_title;
};

// Creates a window container.
// Returns NULL on fail.
struct e_window_container* e_window_container_create(struct e_window* window);

// Sets window container's position relative to its parent tree node.
void e_window_container_set_position(struct e_window_container* window_container, int lx, int ly);

// Configures window container, returning a serial.
uint32_t e_window_container_configure(struct e_window_container* window_container, int lx, int ly, int width, int height);

// Configures window container to take as much space as it can.
// Window container must not be tiled.
uint32_t e_window_container_maximize(struct e_window_container* window_container);

void e_window_container_set_tiled(struct e_window_container* window_container, bool tiled);

// Searches for a window container at the specified layout coords in the given scene graph
// Outs found surface and translated from layout to surface coords.
// If nothing is found returns NULL, but surface may not always be NULL.
struct e_window_container* e_window_container_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy);

void e_window_container_send_close(struct e_window_container* window_container);

void e_window_container_destroy(struct e_window_container* window_container);