#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

struct e_server;

struct e_output;
struct e_seat;

struct e_view;
struct e_layer_surface;

// Functions for
// - Managing a collection of views
// - Desktop focus management (seat handles raw focus)

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

// Returns view currently hovered by cursor.
// Returns NULL if no view is being hovered.
struct e_view* e_desktop_hovered_view(struct e_server* server);

/* focus */

// Set seat focus on a view if possible, and does whatever is necessary to do so.
void e_desktop_focus_view(struct e_view* view);

// Set seat focus on a layer surface if possible.
void e_desktop_focus_layer_surface(struct e_layer_surface* layer_surface);

// Gets the type of surface (view or layer surface) and sets seat focus.
// This will do nothing if surface isn't of a type that can be focused on by the seat
void e_desktop_focus_surface(struct e_server* server, struct wlr_surface* surface);

// Returns view currently in focus.
// Returns NULL if no view has focus.
struct e_view* e_desktop_focused_view(struct e_server* server);

// Returns view previously in focus.
// Returns NULL if no view had focus.
struct e_view* e_desktop_prev_focused_view(struct e_server* server);

void e_desktop_clear_focus(struct e_server* server);
