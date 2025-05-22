#pragma once

#include <stdbool.h>

#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

#include <util/list.h>

//TODO: function for getting the preferred percentage of space a container that hasn't been added yet should take up

struct e_container;
struct e_tree_container;

struct e_container_impl
{
    // Configure the container.
    void (*configure)(struct e_container* container, int lx, int ly, int width, int height);

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

    struct e_container_impl implementation;

    // area container is taking in
    struct wlr_box area;
    // percentage of space this container takes up within parent container
    float percentage;

    // parent of this container, NULL if root container.
    // May be NULL.
    struct e_tree_container* parent;
};

enum e_tiling_mode
{
    E_TILING_MODE_NONE = 0, //Mainly used by root floating container, as it's not supposed to tile
    E_TILING_MODE_HORIZONTAL = 1, //Tile child containers horizontally
    E_TILING_MODE_VERTICAL = 2 //Tile child containers vertically
};

// A container that tiles its children containers.
// Destroys itself if it has no parent when last child is removed.
struct e_tree_container
{
    // How this container should tile containers.
    enum e_tiling_mode tiling_mode;

    struct e_list children; //struct e_container*

    struct e_container base;

    bool destroying;
};

// Returns true on success, false on fail.
bool e_container_init(struct e_container* container, enum e_container_type type, void* data);

void e_container_fini(struct e_container* container);

// Sets the parent of a container.
// Parent may be NULL.
// Returns true on success, false on fail.
bool e_container_set_parent(struct e_container* container, struct e_tree_container* parent);

// Gets next sibling of container.
// Returns NULL if none.
struct e_container* e_container_next_sibling(struct e_container* container);

// Gets previous sibling of container.
// Returns NULL if none.
struct e_container* e_container_prev_sibling(struct e_container* container);

// Configure the container.
void e_container_configure(struct e_container* container, int x, int y, int width, int height);

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
// Tree container is destroyed when no children are left and has a parent.
// Returns true on success, false on fail.
bool e_tree_container_remove_container(struct e_tree_container* tree_container, struct e_container* container);

// Arranges a tree container's children to fit within the container's area.
void e_tree_container_arrange(struct e_tree_container* tree_container);

// Destroy a tree container and free its memory.
void e_tree_container_destroy(struct e_tree_container* tree_container);
