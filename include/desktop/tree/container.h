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

struct e_container;
struct e_tree_container;
struct e_view;

struct e_container_impl
{
    // Arrange container within area.
    void (*arrange)(struct e_container* container, struct wlr_box area);

    // Commit pending changed state to container.
    void (*commit)(struct e_container* container);

    // Destroy the container and free its memory.
    void (*destroy)(struct e_container* container);
};

enum e_container_type
{
    E_CONTAINER_TREE = 1,
    E_CONTAINER_VIEW = 2
};

// A container for autoconfiguring data by its ancestor containers.
struct e_container
{
    enum e_container_type type;

    // Data of this container, usually the struct implementing a container type.
    // May be NULL.
    void* data;

    const struct e_container_impl* implementation;

    // Area containing is taking in.
    struct wlr_box current, pending;
    // percentage of space this container takes up within parent container
    float percentage;

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
    struct e_server* server;

    struct e_view* view;

    // Holds view's tree.
    struct wlr_scene_tree* tree;

    struct e_container base;

    struct wl_listener map;
    struct wl_listener unmap;

    struct wl_listener destroy;

    struct wl_list link; //e_server::view_containers
};

// Returns true on success, false on fail.
bool e_container_init(struct e_container* container, const struct e_container_impl* implementation, enum e_container_type type, void* data);

void e_container_fini(struct e_container* container);

bool e_container_is_tiled(struct e_container* container);

// Sets the parent of a container.
// Parent may be NULL.
// Returns true on success, false on fail.
bool e_container_set_parent(struct e_container* container, struct e_tree_container* parent);

// Arrange container within area.
void e_container_arrange(struct e_container* container, struct wlr_box area);

// Rearrange container within pending area.
void e_container_rearrange(struct e_container* container);

// Commit pending changes to container.
void e_container_commit(struct e_container* container);

// Destroy the container and free its memory.
void e_container_destroy(struct e_container* container);

// Tree container functions

// Creates a tree container.
// Returns NULL on fail.
struct e_tree_container* e_tree_container_create(enum e_tiling_mode tiling_mode);

// Inserts a container in tree container at given index.
// Returns true on success, false on fail.
bool e_tree_container_insert_container(struct e_tree_container* tree_container, struct e_container* container, int index);

// Adds a container to a tree container.
// Returns true on success, false on fail.
bool e_tree_container_add_container(struct e_tree_container* tree_container, struct e_container* container);

// Removes a container from a tree container.
// Returns true on success, false on fail.
bool e_tree_container_remove_container(struct e_tree_container* tree_container, struct e_container* container);

// Arranges a tree container's children to fit within given area.
void e_tree_container_arrange(struct e_tree_container* tree_container, struct wlr_box area);

// Destroy a tree container and free its memory.
void e_tree_container_destroy(struct e_tree_container* tree_container);

// View container functions

// Create a view container.
// Returns NULL on fail.
struct e_view_container* e_view_container_create(struct e_server* server, struct e_view* view);

// Raise view container to the top of its layer. (floating, tiled & fullscreen but only matters for floating)
void e_view_container_raise_to_top(struct e_view_container* view_container);

// Returns NULL on fail.
struct e_view_container* e_view_container_try_from_node_ancestors(struct wlr_scene_node* node);

// Finds the view container at the specified layout coords in given scene graph.
// Returns NULL on fail.
struct e_view_container* e_view_container_at(struct wlr_scene_node* node, double lx, double ly);

// Finds the view container which has this surface as its view's main surface.
// Returns NULL on fail.
struct e_view_container* e_view_container_try_from_surface(struct e_server* server, struct wlr_surface* surface);
