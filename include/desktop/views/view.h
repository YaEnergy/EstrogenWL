#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

#include "desktop/desktop.h"

struct e_view;

//type of an e_view
enum e_view_type
{
    E_VIEW_TOPLEVEL = 1, //struct e_toplevel_view*
    E_VIEW_XWAYLAND = 2 //struct e_xwayland_view*
};

// Implementation of view functions for different view types
struct e_view_impl
{
    void (*set_tiled)(struct e_view* view, bool tiled);
    
    // Configure a view within given layout position and size
    // Returns configure serial, returns 0 if no serial is given
    uint32_t (*configure)(struct e_view* view, int lx, int ly, int width, int height);

    // Request that this view closes
    void (*send_close)(struct e_view* view);
};

// a view: xdg toplevel or xwayland view
struct e_view
{
    struct e_desktop* desktop;

    // Determines what type of view this is: see e_view_type
    enum e_view_type type;
    
    void* data;

    struct e_view_impl implementation;

    // View's content tree
    // May be NULL, if not created
    struct wlr_scene_tree* tree;

    // View's main surface, may be NULL.
    struct wlr_surface* surface;
    // View's current geometry
    struct wlr_box current;

    // Whether the view expects to be in a tiled or floating container.
    bool tiled;

    // May be NULL, if not set
    char* title;

    // May be NULL, if not created
    struct e_window* container;

    struct
    {
        // View title was set.
        struct wl_signal set_title; //char* title
    } events;

    struct wl_list link; //e_desktop::views
};

// Init a view, must call e_view_fini at the end of its life.
// This function should only be called by the implementations of each view type. 
// I mean it would be a bit weird to even call this function somewhere else.
void e_view_init(struct e_view* view, struct e_desktop* desktop, enum e_view_type type, void* data);

// Set view's tree position, relative to container.
void e_view_set_position(struct e_view* view, int lx, int ly);

// Configures a view with the given size.
// Returns configure serial, returns 0 if no serial is given.
uint32_t e_view_set_size(struct e_view* view, int width, int height);

// Sets whether the view thinks it should be tiled or not.
// NOTE: Does not tile its container/window.
void e_view_set_tiled(struct e_view* view, bool tiled);

// Configures a view within given layout position and size
// Returns configure serial, returns 0 if no serial is given
uint32_t e_view_configure(struct e_view* view, int lx, int ly, int width, int height);

// Maximizes view to fit within parent container, must be floating.
void e_view_maximize(struct e_view* view);

// Display view inside a window container.
void e_view_map(struct e_view* view);

// Stop displaying view inside a window container.
void e_view_unmap(struct e_view* view);

// Finds the view which has this surface as its main surface.
// Returns NULL on fail.
struct e_view* e_view_from_surface(struct e_desktop* desktop, struct wlr_surface* surface);

// Returns NULL on fail.
struct e_view* e_view_try_from_node_ancestors(struct wlr_scene_node* node);

// Searches for a view at the specified layout coords in the given scene graph
// Outs found surface and translated from layout to surface coords.
// If nothing is found returns NULL, but surface may not always be NULL.
struct e_view* e_view_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy);

// Requests that this view uses its method of closing.
void e_view_send_close(struct e_view* view);

// Deinit a view.
void e_view_fini(struct e_view* view);