#pragma once

#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

struct e_window;

enum e_tiling_mode
{
    E_TILING_MODE_NONE = 0, //Mainly used by window containers & root floating container, as they only have a window
    E_TILING_MODE_HORIZONTAL = 1, //Tile child containers horizontally
    E_TILING_MODE_VERTICAL = 2 //Tile child containers vertically
};

//a container for multiple other containers, or a single window
struct e_container
{
    //parent of this container, NULL if root container.
    //May be NULL.
    struct e_container* parent;

    //how this container should tile containers
    enum e_tiling_mode tiling_mode;

    //If not NULL, this container holds a window.
    //May be NULL.
    struct e_window* window;

    //containers inside this container
    struct wl_list containers; //struct e_container*

    struct wlr_box area;

    //container tree
    struct wlr_scene_tree* tree;

    //percentage of space this container takes up within parent container
    float percentage;

    struct wl_list link; //e_container::containers

    //tiling container node destroyed
    struct wl_listener destroy;
};

//Creates a container
struct e_container* e_container_create(struct wlr_scene_tree* parent, enum e_tiling_mode tiling_mode);

//Adds a container to a container
void e_container_add_container(struct e_container* container, struct e_container* child_container);

//Removes a container from a container
void e_container_remove_container(struct e_container* container, struct e_container* child_container);

//Sets the parent of a container
void e_container_set_parent(struct e_container* container, struct e_container* parent);

//Creates a container for a window
struct e_container* e_container_window_create(struct wlr_scene_tree* parent, struct e_window* window);

bool e_container_contains_window(struct e_container* container);

//Arranges a containter's children (window or other containers) to fit within the useable area
void e_container_arrange(struct e_container* container, struct wlr_box useable_area);

//Rearranges a container's children (window or other containers) to fit within the USED area
void e_container_rearrange(struct e_container* container);

void e_container_destroy(struct e_container* container);