#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

#include "desktop/desktop.h"

#include "desktop/tree/container.h"
#include "desktop/tree/workspace.h"

//TODO: add functions for states that view considers itself + implementation xdg toplevel & xwayland
//      + editing window container
//  - e_view_set_maximized
//  - e_view_set_resizing
//  - e_view_set_suspended

struct e_view;

// What the view can handle in size.
// 0 or lower means hint isn't set.
struct e_view_size_hints
{
    int min_width;
    int min_height;

    int max_width;
    int max_height;

    int width_inc; //Size increments
    int height_inc; //Size increments
};

//type of an e_view
enum e_view_type
{
    E_VIEW_TOPLEVEL = 1, //struct e_toplevel_view*
    E_VIEW_XWAYLAND = 2 //struct e_xwayland_view*
};

// Implementation of view functions for different view types
struct e_view_impl
{
    // Returns size hints of the view.
    struct e_view_size_hints (*get_size_hints)(struct e_view* view);

    // Notify the view implementation of the new tiled state.
    void (*notify_tiled)(struct e_view* view, bool tiled);

    // Sets the activated state of the view.
    void (*set_activated)(struct e_view* view, bool activated);

    // Set the fullscreen mode of the view.
    void (*set_fullscreen)(struct e_view* view, bool fullscreen);

    //void (*set_maximized)(struct e_view* view, bool maximized);
    //void (*set_resizing)(struct e_view* view, bool resizing);
    
    // Configure a view within given layout position and size.
    void (*configure)(struct e_view* view, int lx, int ly, int width, int height);

    // Create a scene tree displaying this view's surfaces and subsurfaces.
    // Returns NULL on fail.
    // Must be implemented.
    struct wlr_scene_tree* (*create_content_tree)(struct e_view* view);

    // Should return true if the view's container or window should float.
    // If it should tile instead then return false.
    bool (*wants_floating)(struct e_view* view);

    // Request that this view closes
    void (*send_close)(struct e_view* view);
};

// A view: xdg toplevel or xwayland view
struct e_view
{
    struct e_desktop* desktop;

    // Determines what type of view this is: see e_view_type
    enum e_view_type type;
    
    void* data;

    const struct e_view_impl* implementation;

    // View's main surface, may be NULL.
    struct wlr_surface* surface;

    // View's current surface geometry
    struct wlr_box current;
    // View's pending surface geometry
    struct wlr_box pending;

    // View's tree
    struct wlr_scene_tree* tree;
    // View's content tree inside main view tree, displaying its surfaces and subsurfaces.
    // Should always be at position (0, 0), main tree should be moved instead.
    struct wlr_scene_tree* content_tree;

    // Is the view being displayed?
    bool mapped;

    bool tiled;
    bool fullscreen;

    // View's title
    char* title;

    // Workspace view is currently in, may be NULL.
    struct e_workspace* workspace;

    // Base container
    struct e_container container;

    struct wl_list link; //e_desktop::views
};

// Init a view, must call e_view_fini at the end of its life.
// This function should only be called by the implementations of each view type. 
// I mean it would be a bit weird to even call this function somewhere else.
void e_view_init(struct e_view* view, struct e_desktop* desktop, enum e_view_type type, void* data, const struct e_view_impl* implementation);

// Returns size hints of view.
struct e_view_size_hints e_view_get_size_hints(struct e_view* view);

// Display view.
void e_view_map(struct e_view* view);

// Stop displaying view.
void e_view_unmap(struct e_view* view);

// Moves view to a different workspace, and updating its container parent.
// If workspace is NULL, removes view from current workspace.
void e_view_move_to_workspace(struct e_view* view, struct e_workspace* workspace);

// Set pending position of view using layout coordinates.
// Only use if you are moving the view and not resizing it in any way.
void e_view_set_position(struct e_view* view, int lx, int ly);

// Configures a view within given layout position and size.
void e_view_configure(struct e_view* view, int lx, int ly, int width, int height);

// Configure view using pending changes.
void e_view_configure_pending(struct e_view* view);

// Updates view's tree node to current position.
// Should be called when view has moved. (Current x & y changed)
void e_view_moved(struct e_view* view);

// Sets the tiled state of the view.
void e_view_set_tiled(struct e_view* view, bool tiled);

// Sets the activated state of the view.
void e_view_set_activated(struct e_view* view, bool activated);

void e_view_fullscreen(struct e_view* view);
void e_view_unfullscreen(struct e_view* view);

// Set the fullscreen mode of the view.
void e_view_set_fullscreen(struct e_view* view, bool fullscreen);

/*
void e_view_set_maximized(struct e_view* view, bool maximized);
void e_view_set_resizing(struct e_view* view, bool resizing);
void e_view_set_suspended(struct e_view* view, bool suspended);
*/

bool e_view_has_pending_changes(struct e_view* view);

// Finds the view which has this surface as its main surface.
// Returns NULL on fail.
struct e_view* e_view_from_surface(struct e_desktop* desktop, struct wlr_surface* surface);

// Returns NULL on fail.
struct e_view* e_view_try_from_node_ancestors(struct wlr_scene_node* node);

// Finds the view at the specified layout coords in given scene graph.
// Returns NULL if nothing is found.
struct e_view* e_view_at(struct wlr_scene_node* node, double lx, double ly);

// Requests that this view uses its method of closing.
void e_view_send_close(struct e_view* view);

// Deinit a view.
void e_view_fini(struct e_view* view);
