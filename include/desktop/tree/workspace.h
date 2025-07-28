#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/util/box.h>

#include <wlr/types/wlr_scene.h>

#include "desktop/tree/container.h"

#include "util/list.h"

struct e_output;
struct e_view;

struct e_ext_workspace;
struct e_cosmic_workspace;

struct e_workspace_layers
{
    struct wlr_scene_tree* tiling;
    struct wlr_scene_tree* floating;
    struct wlr_scene_tree* fullscreen;
};

// A virtual desktop, containing a group of views that can be displayed with by a single output.
struct e_workspace
{
    struct e_output* output;

    // Is an output displaying this workspace?
    bool active;

    struct e_workspace_layers layers;

    struct wlr_box full_area;
    struct wlr_box tiled_area;
    
    // View currently in fullscreen mode.
    struct e_view* fullscreen_view;

    //container for tiled containers
    struct e_tree_container* root_tiling_container;

    struct e_list floating_containers; //struct e_container*

    struct e_cosmic_workspace* cosmic_handle;

    struct wl_listener cosmic_request_activate;

    struct e_ext_workspace* ext_handle;

    struct wl_listener ext_request_activate;
};

// Create a new workspace for an output.
// Returns NULL on fail.
struct e_workspace* e_workspace_create(struct e_output* output);

// Set name of workspace.
void e_workspace_set_name(struct e_workspace* workspace, const char* name);

// Enable/disable workspace trees.
void e_workspace_set_activated(struct e_workspace* workspace, bool activated);

// Arranges a workspace's children to fit within the given area.
void e_workspace_arrange(struct e_workspace* workspace, struct wlr_box full_area, struct wlr_box tiled_area);

// Update visibility of workspace trees.
void e_workspace_update_tree_visibility(struct e_workspace* workspace);

// Adds container as tiled to workspace.
// Workspace must be arranged.
void e_workspace_add_tiled_container(struct e_workspace* workspace, struct e_container* container);

// Adds container as floating to workspace.
// Workspace must be arranged.
void e_workspace_add_floating_container(struct e_workspace* workspace, struct e_container* container);

// Get workspace from node ancestors.
// Returns NULL on fail.
struct e_workspace* e_workspace_try_from_node_ancestors(struct wlr_scene_node* node);

// Destroy the workspace.
void e_workspace_destroy(struct e_workspace* workspace);