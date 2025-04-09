#pragma once

#include <stdbool.h>

#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

struct e_container;
struct e_tree_container;

struct e_container_impl
{
    // Configure the container.
    // Returns configure serial, returns 0 if no serial is given.
    uint32_t (*configure)(struct e_container* container, int lx, int ly, int width, int height);

    // Destroy the container.
    void (*destroy)(struct e_container* container);
};

// A container for autoconfiguring data by its ancestor containers.
struct e_container
{
    // Data of this container, usually the struct implementing a container type.
    // May be NULL.
    void* data;

    struct e_container_impl implementation;

    // area container is taking in
    struct wlr_box area;
    // percentage of space this container takes up within parent container
    float percentage;

    struct wl_list link; // e_tree_container::children

    // parent of this container, NULL if root container.
    // May be NULL.
    struct e_tree_container* parent;

    // container tree
    struct wlr_scene_tree* tree;
};

enum e_tiling_mode
{
    E_TILING_MODE_NONE = 0, //Mainly used by window containers & root floating container, as they only have a window
    E_TILING_MODE_HORIZONTAL = 1, //Tile child containers horizontally
    E_TILING_MODE_VERTICAL = 2 //Tile child containers vertically
};

// A container that tiles its children containers.
// Destroys itself if it has no parent when last child is removed.
struct e_tree_container
{
    // How this container should tile containers.
    enum e_tiling_mode tiling_mode;

    struct wl_list children; //struct e_container*

    struct e_container base;

    bool destroying;
};

// Returns true on success, false on fail.
bool e_container_init(struct e_container* container, struct wlr_scene_tree* parent, void* data);

void e_container_fini(struct e_container* container);

// Sets the parent of a container.
// Returns true on success, false on fail.
bool e_container_set_parent(struct e_container* container, struct e_tree_container* parent);

// Sets the position of container relative to the parent node.
void e_container_set_position(struct e_container* container, int lx, int ly);

// Configure the container.
// Returns configure serial, returns 0 if no serial was given.
uint32_t e_container_configure(struct e_container* container, int lx, int ly, int width, int height);

// Tree container functions

// Creates a tree container.
// Returns NULL on fail.
struct e_tree_container* e_tree_container_create(struct wlr_scene_tree* parent, enum e_tiling_mode tiling_mode);

// Adds a container to a tree container.
// Returns true on success, false on fail.
bool e_tree_container_add_container(struct e_tree_container* tree_container, struct e_container* container);

// Removes a container from a tree container.
// Tree container is destroyed when no children are left and has a parent.
// Returns true on success, false on fail.
bool e_tree_container_remove_container(struct e_tree_container* tree_container, struct e_container* container);

// Arranges a tree container's children to fit within the container's area.
void e_tree_container_arrange(struct e_tree_container* tree_container);

// Destroy a tree container and free its memory.
void e_tree_container_destroy(struct e_tree_container* tree_container);