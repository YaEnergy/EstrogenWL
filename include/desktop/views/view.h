#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

#include "desktop/tree/container.h"
#include "desktop/tree/workspace.h"

//TODO: add functions for states that view considers itself + implementation xdg toplevel & xwayland
//      + editing window container
//  - e_view_set_maximized
//  - e_view_set_resizing
//  - e_view_set_suspended

struct e_output;

struct e_foreign_toplevel;
struct e_view;

// What the view can handle in size.
// 0 or lower means hint isn't set.
struct e_view_size_hints
{
    int min_width, min_height;
    int max_width, max_height;

    int width_inc, height_inc; //Size increments
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

    // Sets the tiled state of the view.
    void (*set_tiled)(struct e_view* view, bool tiled);

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

// View is ready to be displayed.
struct e_view_map_event
{
    struct e_view* view;
    
    bool fullscreen;
    struct e_output* fullscreen_output; //may be NULL

    bool wants_floating;
};

// View wants to set fullscreen mode.
struct e_view_request_fullscreen_event
{
    struct e_view* view;
    bool fullscreen;
    struct e_output* output; //may be NULL
};

// View wants to start an interactive move action.
struct e_view_request_move_event
{
    struct e_view* view;
};

// View wants to start an interactive resize action.
struct e_view_request_resize_event
{
    struct e_view* view;
    uint32_t edges; //bitmask enum wlr_edges
};

// View requests a specific configure.
struct e_view_request_configure_event
{
    struct e_view* view;
    int x;
    int y;
    int width;
    int height;
};

// A view: xdg toplevel or xwayland view
struct e_view
{
    // Server that created this view.
    struct e_server* server;

    // Determines what type of view this is: see e_view_type
    enum e_view_type type;
    
    void* data;

    const struct e_view_impl* implementation;

    // View's main surface, may be NULL.
    struct wlr_surface* surface;
    // View's current root surface geometry
    struct wlr_box root_geometry;

    // Size of view, includes subsurfaces.
    int width, height;
    // Space for popups relative to view.
    // Note: not relative to root toplevel surface, but to toplevels (0, 0) point. So no need to access view's geometry x & y when setting this.
    struct wlr_box popup_space;

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

    // Output where view is currently being displayed, may be NULL.
    struct e_output* output;

    // May be NULL.
    struct e_foreign_toplevel* foreign_toplevel;

    struct
    {
        // View is ready to be displayed.
        struct wl_signal map; //struct e_view_map_event
        struct wl_signal unmap;

        // View committed surface state.
        struct wl_signal commit;

        // View wants to set fullscreen mode.
        // View must be configured, even if nothing changes.
        struct wl_signal request_fullscreen; //struct e_view_request_fullscreen_event
        // View wants to start an interactive move action.
        struct wl_signal request_move; //struct e_view_request_move_event
        // View wants to start an interactive resize action.
        struct wl_signal request_resize; //struct e_view_request_resize_event
        // View requests a specific configure.
        // View must be configured, even if nothing changes.
        struct wl_signal request_configure; //struct e_view_request_configure_event
        // View wants to be activated.
        struct wl_signal request_activate;

        struct wl_signal destroy;
    } events;
};

// Init a view, must call e_view_fini at the end of its life.
// This function should only be called by the implementations of each view type. 
// I mean it would be a bit weird to even call this function somewhere else.
void e_view_init(struct e_view* view, enum e_view_type type, void* data, const struct e_view_impl* implementation, struct e_server* server);

// Returns size hints of view.
struct e_view_size_hints e_view_get_size_hints(struct e_view* view);

// Display view within view's tree & emit map signal.
// Set fullscreen to true and set fullscreen_output if you want to request that the view should be on a specific output immediately.
// Output is allowed to be NULL.
void e_view_map(struct e_view* view, bool fullscreen, struct e_output* fullscreen_output);

// Stop displaying view within view's tree & emit unmap signal.
void e_view_unmap(struct e_view* view);

// Sets output of view.
// Output is allowed to be NULL.
void e_view_set_output(struct e_view* view, struct e_output* output);

// Set space for popups relative to view.
// Note: not relative to root toplevel surface, but to toplevels (0, 0) point. So no need to access view's geometry when setting this.
void e_view_set_popup_space(struct e_view* view, struct wlr_box toplevel_popup_space);

// Configures a view within given layout position and size.
void e_view_configure(struct e_view* view, int lx, int ly, int width, int height);

// Sets the tiled state of the view.
void e_view_set_tiled(struct e_view* view, bool tiled);

// Sets the activated state of the view.
void e_view_set_activated(struct e_view* view, bool activated);

// Set the fullscreen mode of the view.
void e_view_set_fullscreen(struct e_view* view, bool fullscreen);

/*
void e_view_set_maximized(struct e_view* view, bool maximized);
void e_view_set_resizing(struct e_view* view, bool resizing);
void e_view_set_suspended(struct e_view* view, bool suspended);
*/

// Returns NULL on fail.
struct e_view* e_view_try_from_node_ancestors(struct wlr_scene_node* node);

// Finds the view at the specified layout coords in given scene graph.
// Returns NULL if nothing is found.
struct e_view* e_view_at(struct wlr_scene_node* node, double lx, double ly);

// Requests that this view uses its method of closing.
void e_view_send_close(struct e_view* view);

// Deinit a view.
void e_view_fini(struct e_view* view);
