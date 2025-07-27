#pragma once

#include <stdbool.h>

#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

#include <util/list.h>

struct e_server;

struct e_workspace;
struct e_view;
struct e_container;
struct e_view_container;
struct e_tree_container;

enum e_container_type
{
    E_CONTAINER_TREE = 1, //tree_container
    E_CONTAINER_VIEW = 2 //view_container
};

// A container for autoconfiguring data by its ancestor containers.
struct e_container
{
    struct e_server* server;

    enum e_container_type type;

    // Container implementation, see type (e_container_type).
    // May be NULL.
    union
    {
        struct e_tree_container* tree_container;
        struct e_view_container* view_container;
    };

    // Area containing is taking in.
    struct wlr_box area;
    // percentage of space this container takes up within parent container
    float percentage;

    struct wlr_scene_tree* tree;

    struct e_workspace* workspace;

    // Parent of this container, NULL if root container or floating container.
    // May be NULL.
    struct e_tree_container* parent;
};

enum e_tiling_mode
{
    E_TILING_MODE_HORIZONTAL, //Tile child containers horizontally
    E_TILING_MODE_VERTICAL //Tile child containers vertically
};

// A container that tiles its children containers.
struct e_tree_container
{
    // How this container should tile containers.
    enum e_tiling_mode tiling_mode;

    struct e_list children; //struct e_container*

    struct e_container base;

    bool destroying;
};

// A container that contains a single view.
struct e_view_container
{
    struct e_view* view;

    // View's pending & current area.
    struct wlr_box view_current, view_pending;

    struct e_container base;

    struct wl_listener map;
    struct wl_listener unmap;

    struct wl_listener destroy;

    struct wl_list link; //e_server::view_containers
};

// Returns true on success, false on fail.
bool e_container_init(struct e_container* container, enum e_container_type type, struct e_server* server);

void e_container_fini(struct e_container* container);

bool e_container_is_tiled(struct e_container* container);

void e_container_set_workspace(struct e_container* container, struct e_workspace* workspace);

// Sets the parent of a container.
// Parent may be NULL.
// Returns true on success, false on fail.
bool e_container_set_parent(struct e_container* container, struct e_tree_container* parent);

// Arrange container within area.
void e_container_arrange(struct e_container* container, struct wlr_box area);

// Rearrange container within pending area.
void e_container_rearrange(struct e_container* container);

void e_container_raise_to_top(struct e_container* container);

// Returns NULL on fail.
struct e_container* e_container_try_from_node_ancestors(struct wlr_scene_node* node);

// Finds the view container at the specified layout coords in given scene graph.
// Returns NULL on fail.
struct e_container* e_container_at(struct wlr_scene_node* node, double lx, double ly);

// Destroy the container and free its memory.
void e_container_destroy(struct e_container* container);

// Tree container functions

// Creates a tree container.
// Returns NULL on fail.
struct e_tree_container* e_tree_container_create(struct e_server* server, enum e_tiling_mode tiling_mode);

// Inserts a container in tree container at given index.
// Returns true on success, false on fail.
bool e_tree_container_insert_container(struct e_tree_container* tree_container, struct e_container* container, int index);

// Adds a container to a tree container.
// Returns true on success, false on fail.
bool e_tree_container_add_container(struct e_tree_container* tree_container, struct e_container* container);

// Removes a container from a tree container.
// Returns true on success, false on fail.
bool e_tree_container_remove_container(struct e_tree_container* tree_container, struct e_container* container);

// View container functions

// Create a view container.
// Returns NULL on fail.
struct e_view_container* e_view_container_create(struct e_server* server, struct e_view* view);

// Finds the view container which has this surface as its view's main surface.
// Returns NULL on fail.
struct e_view_container* e_view_container_try_from_surface(struct e_server* server, struct wlr_surface* surface);
