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

/* hover */

// Returns output currently hovered by cursor.
// Returns NULL if no output is being hovered.
struct e_output* e_desktop_hovered_output(struct e_server* server);

// Returns container currently hovered by cursor.
// Returns NULL if no container is being hovered.
struct e_container* e_desktop_hovered_container(struct e_server* server);

/* focus */

// Sets desktop's current seat's focus to given view container.
// view_container is allowed to be NULL.
void e_desktop_set_focus_view_container(struct e_server* server, struct e_view_container* view_container);

// Sets desktop's current seat's focus to given layer surface.
// layer_surface is allowed to be NULL.
void e_desktop_set_focus_layer_surface(struct e_server* server, struct e_layer_surface* layer_surface);

// Returns view container currently in focus.
// Returns NULL if no view container has focus.
struct e_view_container* e_desktop_focused_view_container(struct e_server* server);

/* interactive */

// Starts an interactive container resize action.
//TODO: void e_desktop_start_interactive_container_resize(struct e_container* container, uint32_t edges);

// Starts an interactive container move action.
//TODO: void e_desktop_start_interactive_container_move(struct e_container* container);

// Ends the current interactive action.
//TODO: void e_desktop_end_interactive(struct e_server* server);
