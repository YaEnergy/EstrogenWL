#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

//TODO: interactive container action functions here instead of in cursor, #include <wlr/util/edges.h>

struct e_server;

struct e_output;
struct e_seat;

struct e_container;
struct e_view_container; //TODO: remove, only use e_container here
struct e_layer_surface;

// Functions for
// - Managing a collection of views
// - Desktop focus management (seat handles raw focus)

struct e_desktop_state
{
    // Layer surface with focus.
    struct e_layer_surface* focused_layer_surface;

    // View container currently considered active.
    // This receives focus as long as there is no focused layer surface.
    struct e_view_container* active_view_container;
};

void e_desktop_state_init(struct e_desktop_state* desktop_state);

/* scene */

// Finds the scene surface at the specified layout coords in given scene graph.
// Also translates the layout coords to the surface coords if not NULL. (sx, sy)
// NULL for sx & sy is allowed.
// Returns NULL if nothing is found.
struct wlr_scene_surface* e_desktop_scene_surface_at(struct wlr_scene_node* node, double lx, double ly, double* sx, double* sy);

/* hover */

// Returns output currently hovered by cursor.
// Returns NULL if no output is being hovered.
struct e_output* e_desktop_hovered_output(struct e_server* server);

// Returns container currently hovered by cursor.
// Returns NULL if no container is being hovered.
struct e_container* e_desktop_hovered_container(struct e_server* server);

/* focus */

// Set seat focus on a view container if possible, and does whatever is necessary to do so.
void e_desktop_focus_view_container(struct e_view_container* view_container);

// Set seat focus on a layer surface if possible.
void e_desktop_focus_layer_surface(struct e_layer_surface* layer_surface);

// Gets the type of surface (view container or layer surface) and sets seat focus.
// This will do nothing if surface isn't of a type that can be focused on by the seat
void e_desktop_focus_surface(struct e_server* server, struct wlr_surface* surface);

// Returns view container currently in focus.
// Returns NULL if no view container has focus.
struct e_view_container* e_desktop_focused_view_container(struct e_server* server);

void e_desktop_clear_focus(struct e_server* server);

/* interactive */

// Starts an interactive container resize action.
//TODO: void e_desktop_start_interactive_container_resize(struct e_container* container, uint32_t edges);

// Starts an interactive container move action.
//TODO: void e_desktop_start_interactive_container_move(struct e_container* container);

// Ends the current interactive action.
//TODO: void e_desktop_end_interactive(struct e_server* server);
