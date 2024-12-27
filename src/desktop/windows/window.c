#include "desktop/windows/window.h"

#include <stdlib.h>

#include "desktop/surface.h"

#include "util/log.h"

#include "wm.h"

//this function should only be called by the implementations of each window type. 
//I mean it would be a bit weird to even call this function somewhere else.
struct e_window* e_window_create(struct e_server* server, enum e_window_type type)
{
    struct e_window* window = calloc(1, sizeof(struct e_window));
    window->server = server;
    window->type = type;

    window->toplevel_window = NULL;

    window->scene_tree = NULL;

    wl_list_init(&window->link);

    return window;
}

void e_window_destroy(struct e_window *window)
{
    free(window);
}

void e_window_set_position(struct e_window* window, int x, int y)
{
    if (window->scene_tree == NULL)
    {
        e_log_error("Can't set position of window, window has no scene tree!");
        return;
    }

    wlr_scene_node_set_position(&window->scene_tree->node, x, y);
}

void e_window_set_size(struct e_window* window, int32_t x, int32_t y)
{
    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            wlr_xdg_toplevel_set_size(window->toplevel_window->xdg_toplevel, x, y);
            break;
        default:
            e_log_error("Can't set size of window, window is an unsupported type!");
            break;
    }
}

void e_window_map(struct e_window *window)
{
    wl_list_insert(&window->server->windows, &window->link);

    e_tile_windows(window->server);
}

void e_window_unmap(struct e_window *window)
{
    wl_list_remove(&window->link);

    e_tile_windows(window->server);
}

struct wlr_surface* e_window_get_surface(struct e_window* window)
{
    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            return window->toplevel_window->xdg_toplevel->base->surface;
        default:
            e_log_error("Can't get surface of window, window is an unsupported type!");
            return NULL;
    }
}

struct e_window* e_window_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy)
{
    struct wlr_scene_node* snode;
    *surface = e_wlr_surface_at(node, lx, ly, &snode, sx, sy);

    if (snode == NULL || *surface == NULL)
        return NULL;
        
    //keep going up the tree, until our parent is the root of the tree, 
    //which means we'll have found the top window
    while (snode != NULL && snode->parent != NULL && snode->parent->node.parent != NULL)
        snode = &snode->parent->node;

    if (snode == NULL)
        return NULL;

    return snode->data; //struct e_window*
}