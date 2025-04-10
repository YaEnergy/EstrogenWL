#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <wayland-server-core.h>

#include <wayland-util.h>
#include <wlr/types/wlr_scene.h>

#include "desktop/tree/container.h"
#include "desktop/views/view.h"

// Container for views that allows to make themselves visible.
struct e_window
{
    struct e_desktop* desktop;
    
    char* title;

    struct wlr_scene_rect* background;

    bool tiled;
    
    struct e_view* view;

    struct e_container base;

    struct wl_listener view_set_title;
};

// Creates a window for a view.
// Returns NULL on fail.
struct e_window* e_window_create(struct e_view* view);

// Sets window's position relative to its parent tree node.
void e_window_set_position(struct e_window* window, int lx, int ly);

// Configures window container, returning a serial.
uint32_t e_window_configure(struct e_window* window, int lx, int ly, int width, int height);

// Configures window container to take as much space as it can.
// Window container must not be tiled.
uint32_t e_window_maximize(struct e_window* window);

void e_window_set_tiled(struct e_window* window, bool tiled);

// Returns NULL on fail.
struct e_window* e_window_try_from_node(struct wlr_scene_node* node);

// Returns NULL on fail.
struct e_window* e_window_try_from_node_ancestors(struct wlr_scene_node* node);

// Searches for a window container at the specified layout coords in the given scene graph
// Outs found surface and translated from layout to surface coords.
// If nothing is found returns NULL, but surface may not always be NULL.
struct e_window* e_window_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy);

void e_window_send_close(struct e_window* window);

void e_window_destroy(struct e_window* window);