#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

#include "desktop/windows/toplevel_window.h"
#include "desktop/windows/xwayland_window.h"

#include "server.h"

// 2024-12-26 23:13:08 unsure why I made these comments so much more detailed than others, I should do that more often

//TODO: xwayland support

//type of an e_window
enum e_window_type
{
    E_WINDOW_TOPLEVEL,
    E_WINDOW_XWAYLAND
};

//a window: xdg toplevel or xwayland window
struct e_window
{
    struct e_server* server;

    //Determines what type of window this is: see e_window_type
    enum e_window_type type;
    
    union
    {
        //Only a valid pointer if type == E_WINDOW_TOPLEVEL
        struct e_toplevel_window* toplevel_window;
        //Only a valid pointer if type == E_WINDOW_XWAYLAND
        struct e_xwayland_window* xwayland_window;
    };

    struct wlr_scene_tree* scene_tree;

    bool tiled;

    //to link to linked lists
    struct wl_list link;
};

//this function should only be called by the implementations of each window type. 
//I mean it would be a bit weird to even call this function somewhere else.
struct e_window* e_window_create(struct e_server* server, enum e_window_type type);

void e_window_create_scene_tree(struct e_window* window, struct wlr_scene_tree* parent);

//returns pointer to window's title, NULL on fail
char* e_window_get_title(struct e_window* window);

//outs top left x and y of window
void e_window_get_position(struct e_window* window, int* x, int* y);

//outs width (x) and height (y) of window, pointers are set to -1 on fail
void e_window_get_size(struct e_window* window, int* x, int* y);

//returns the window's main surface geometry
struct wlr_box e_window_get_main_geometry(struct e_window* window);

void e_window_set_position(struct e_window* window, int x, int y);

//includes decoration in x & y
void e_window_set_size(struct e_window* window, int32_t x, int32_t y);

//does not include decoration in x & y, only client geometry
void e_window_set_bounds(struct e_window* window, int32_t x, int32_t y);

void e_window_set_tiled(struct e_window* window, bool tiled);

//display window
void e_window_map(struct e_window* window);

//stop displaying window
void e_window_unmap(struct e_window* window);

//gets the window's main surface
struct wlr_surface* e_window_get_surface(struct e_window* window);

//finds the window which has this surface as its main surface, NULL if not found
struct e_window* e_window_from_surface(struct e_server* server, struct wlr_surface* surface);

//Searches for a window at the specified layout coords in the given scene graph
//Outs found surface and translated from layout to surface coords.
//If nothing is found returns NULL, but surface may not always be NULL.
struct e_window* e_window_at(struct e_server* server, struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy);

//Requests that this window uses its method of closing
void e_window_send_close(struct e_window* window);

//Destroys the base window and frees up its used memory
void e_window_destroy(struct e_window* window);