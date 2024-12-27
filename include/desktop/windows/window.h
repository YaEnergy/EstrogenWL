#pragma once

#include <stdint.h>

#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

#include "desktop/windows/toplevel_window.h"

#include "server.h"

// 2024-12-26 23:13:08 unsure why I made these comments so much more detailed than others, I should do that more often

//TODO: xwayland support

//type of an e_window
enum e_window_type
{
    E_WINDOW_TOPLEVEL
    //E_WINDOW_XWAYLAND
};

//a window: xdg toplevel, xdg popup or (currently unimplemented) xwayland window
struct e_window
{
    struct e_server* server;

    //Determines what type of window this is: see e_window_type
    enum e_window_type type;
    
    //Only a valid pointer if type == E_WINDOW_TOPLEVEL
    struct e_toplevel_window* toplevel_window;

    struct wlr_scene_tree* scene_tree;

    //to link to linked lists
    struct wl_list link;
};

//this function should only be called by the implementations of each window type. 
//I mean it would be a bit weird to even call this function somewhere else.
struct e_window* e_window_create(struct e_server* server, enum e_window_type type);

//Requests that this window uses its method of closing
void e_window_request_close(struct e_window* window);

//Completely destroys the window and frees up its used memory
void e_window_destroy(struct e_window* window);

void e_window_set_position(struct e_window* window, int x, int y);

void e_window_set_size(struct e_window* window, int32_t x, int32_t y);

void e_window_map(struct e_window* window);

void e_window_unmap(struct e_window* window);

//gets the window's main surface
struct wlr_surface* e_window_get_surface(struct e_window* window);

//Searches for a window at the specified layout coords in the given scene graph
//Outs found surface and translated from layout to surface coords.
//If nothing is found returns NULL, but surface may not always be NULL.
struct e_window* e_window_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy);