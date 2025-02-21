#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

#include "desktop/tree/container.h"

#include "server.h"

//type of an e_window
enum e_window_type
{
    E_WINDOW_TOPLEVEL = 1, //struct e_toplevel_window*
    E_WINDOW_XWAYLAND = 2 //struct e_xwayland_window*
};

// Implementation of window functions for different window types
struct e_window_impl
{
    void (*changed_tiled)(struct e_window* window, bool tiled);
    
    // Configure a window within given layout position and size
    // Returns configure serial, returns 0 if no serial is given
    uint32_t (*configure)(struct e_window* window, int lx, int ly, int width, int height);

    // Request that this window closes
    void (*send_close)(struct e_window* window);
};

//a window: xdg toplevel or xwayland window
struct e_window
{
    struct e_server* server;

    //Determines what type of window this is: see e_window_type
    enum e_window_type type;
    
    void* data;

    struct e_window_impl implementation;

    //May be NULL, if not created
    struct wlr_scene_tree* tree;

    // Window's main surface, may be NULL.
    struct wlr_surface* surface;
    // Window's current geometry
    struct wlr_box current;

    bool tiled;

    //May be NULL, if not set
    char* title;

    //May be NULL, if not created
    struct e_container* container;

    struct wl_list link; //e_scene::windows
};

//this function should only be called by the implementations of each window type. 
//I mean it would be a bit weird to even call this function somewhere else.
struct e_window* e_window_create(struct e_server* server, enum e_window_type type);

void e_window_create_container_tree(struct e_window* window, struct wlr_scene_tree* parent);

void e_window_destroy_container_tree(struct e_window* window);

//set window's tree position, relative to container
void e_window_set_position(struct e_window* window, int lx, int ly);

// Configures a window with the given size
// Returns configure serial, returns 0 if no serial is given
uint32_t e_window_set_size(struct e_window* window, int width, int height);

void e_window_set_tiled(struct e_window* window, bool tiled);

void e_window_base_set_tiled(struct e_window* window, bool tiled);

// Configures a window within given layout position and size
// Returns configure serial, returns 0 if no serial is given
uint32_t e_window_configure(struct e_window* window, int lx, int ly, int width, int height);

//maximizes window to fit within parent container, must be floating
void e_window_maximize(struct e_window* window);

//display window
void e_window_map(struct e_window* window);

//stop displaying window
void e_window_unmap(struct e_window* window);

//finds the window which has this surface as its main surface, NULL if not found
struct e_window* e_window_from_surface(struct e_server* server, struct wlr_surface* surface);

//Returns NULL on fail.
struct e_window* e_window_try_from_node_ancestors(struct wlr_scene_node* node);

//Searches for a window at the specified layout coords in the given scene graph
//Outs found surface and translated from layout to surface coords.
//If nothing is found returns NULL, but surface may not always be NULL.
struct e_window* e_window_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy);

//Requests that this window uses its method of closing
void e_window_send_close(struct e_window* window);

//Destroys the base window and frees up its used memory
void e_window_destroy(struct e_window* window);