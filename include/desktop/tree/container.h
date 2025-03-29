#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

enum e_tiling_mode
{
    E_TILING_MODE_NONE = 0, //Mainly used by window containers & root floating container, as they only have a window
    E_TILING_MODE_HORIZONTAL = 1, //Tile child containers horizontally
    E_TILING_MODE_VERTICAL = 2 //Tile child containers vertically
};

struct e_container;

struct e_container_impl
{
    // Auto-configures the container's data
    // Returns configure serial, returns 0 if no serial is given
    uint32_t (*autoconfigure_data)(struct e_container* container);
};

// a container for multiple other containers, or a single window
struct e_container
{
    // how this container should tile containers
    enum e_tiling_mode tiling_mode;

    // data of this container, usually the one using this container.
    // May be NULL.
    void* data;

    // containers inside this container
    struct wl_list containers; //struct e_container*

    struct e_container_impl implementation;

    // area container is taking in
    struct wlr_box area;
    // percentage of space this container takes up within parent container
    float percentage;

    struct wl_list link; // e_container::containers

    // parent of this container, NULL if root container.
    // May be NULL.
    struct e_container* parent;

    // container tree
    struct wlr_scene_tree* tree;

    // tiling container node destroyed
    struct wl_listener destroy;
    
    bool destroying;
};

// Creates a container
struct e_container* e_container_create(struct wlr_scene_tree* parent, enum e_tiling_mode tiling_mode);

// Adds a container to a container
void e_container_add_container(struct e_container* container, struct e_container* child_container);

// Removes a container from a container
void e_container_remove_container(struct e_container* container, struct e_container* child_container);

// Sets the parent of a container
void e_container_set_parent(struct e_container* container, struct e_container* parent);

// Sets the position of container relative to the parent node
void e_container_set_position(struct e_container* container, int lx, int ly);

void e_container_configure(struct e_container* container, int lx, int ly, int width, int height);

// Arranges a containter's children (window or other containers) to fit within the container's area
void e_container_arrange(struct e_container* container);

void e_container_destroy(struct e_container* container);