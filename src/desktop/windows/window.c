#include "desktop/windows/window.h"

#include <stdlib.h>

#include <wayland-util.h>

#include <wlr/util/edges.h>

#include "desktop/scene.h"
#include "desktop/windows/toplevel_window.h"
#include "input/seat.h"
#include "input/cursor.h"
#include "server.h"
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

void e_window_init_xdg_scene_tree(struct e_window* window, struct wlr_scene_tree* parent, struct wlr_xdg_surface* xdg_surface)
{
    //create scene xdg surface for xdg toplevel and window, and set up window scene tree
    window->scene_tree = wlr_scene_xdg_surface_create(parent, xdg_surface);
    window->scene_tree->node.data = window;

    //allows popup scene trees to add themselves to this window's scene tree
    xdg_surface->data = window->scene_tree;
}

char* e_window_get_title(struct e_window* window)
{
    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            return window->toplevel_window->xdg_toplevel->title;
        default:
            e_log_error("Can't get title of window, window is an unsupported type!");
            return NULL;
    }
}

//outs top left x and y of window, pointers are set to -1 on fail
void e_window_get_position(struct e_window* window, int* x, int* y)
{
    if (x == NULL || y == NULL)
    {
        e_log_error("e_window_get_position: please give valid pointers");
        abort();
    }

    if (window->scene_tree == NULL)
    {
        e_log_error("e_window_get_position: window has no scene tree");
        abort();
    }

    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            *x = window->scene_tree->node.x;
            *y = window->scene_tree->node.y;
            break;
        default:
            *x = -1;
            *y = -1;
            e_log_error("Can't get size of window, window is an unsupported type!");
            break;
    }
}

//outs width (x) and height (y) of window, x and y are set to -1 on fail
void e_window_get_size(struct e_window* window, int* x, int* y)
{
    if (x == NULL || y == NULL)
    {
        e_log_error("e_window_get_size: please give valid pointers");
        abort();
    }

    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            *x = window->toplevel_window->xdg_toplevel->current.width;
            *y = window->toplevel_window->xdg_toplevel->current.height;
            break;
        default:
            *x = -1;
            *y = -1;
            e_log_error("Can't get size of window, window is an unsupported type!");
            break;
    }
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
            //don't resize the toplevel to the same size
            if (window->toplevel_window->xdg_toplevel->current.width == x && window->toplevel_window->xdg_toplevel->current.height == y)
                return;

            wlr_xdg_toplevel_set_size(window->toplevel_window->xdg_toplevel, x, y);
            break;
        default:
            e_log_error("Can't set size of window, window is an unsupported type!");
            break;
    }
}

void e_window_set_tiled(struct e_window* window, bool tiled)
{
    e_log_info("setting tiling mode of window to %B...", tiled);

    if (window->tiled == tiled)
        return;

    window->tiled = tiled;

    if (window->scene_tree != NULL)
    {
        wlr_scene_node_reparent(&window->scene_tree->node, tiled ? window->server->scene->layers.tiling : window->server->scene->layers.floating);
        wlr_scene_node_raise_to_top(&window->scene_tree->node);
    }  

    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            wlr_xdg_toplevel_set_tiled(window->toplevel_window->xdg_toplevel, tiled ? WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP : WLR_EDGE_NONE);
            break;
        default:
            e_log_error("Can't properly set tiled mode of window, window is an unsupported type!");
            break;
    }

    //retile all windows, if this window is mapped
    struct wlr_surface* window_surface = e_window_get_surface(window);
    if (window_surface != NULL && window_surface->mapped)
        e_tile_windows(window->server);
}

void e_window_map(struct e_window* window)
{
    e_log_info("map");

    wl_list_insert(&window->server->scene->windows, &window->link);

    if (window->tiled)
        e_tile_windows(window->server);

    struct wlr_surface* window_surface = e_window_get_surface(window);

    //set focus to this window's main surface
    if (window_surface != NULL)
        e_seat_set_focus(window->server->input_manager->seat, window_surface);
}

void e_window_unmap(struct e_window* window)
{
    wl_list_remove(&window->link);

    if (window->tiled)
        e_tile_windows(window->server);

    struct wlr_surface* window_surface = e_window_get_surface(window);

    //if this window's surface had focus, clear it
    if (window_surface != NULL && e_seat_has_focus(window->server->input_manager->seat, window_surface))
        e_seat_clear_focus(window->server->input_manager->seat);

    //if this window was grabbed by the cursor make it let go
    if (window->server->input_manager->cursor->grab_window == window)
        e_cursor_reset_mode(window->server->input_manager->cursor);
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

struct e_window* e_window_from_surface(struct e_server* server, struct wlr_surface* surface)
{
    if (wl_list_empty(&server->scene->windows))
        return NULL;

    struct e_window* window;

    wl_list_for_each(window, &server->scene->windows, link)
    {
        struct wlr_surface* window_surface = e_window_get_surface(window);

        if (window_surface == NULL)
            continue;
        
        //found window with given surface as main surface
        if (window_surface == surface)
            return window;
    }

    //none found
    return NULL; 
}

struct e_window* e_window_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy)
{
    struct wlr_scene_node* snode;
    *surface = e_scene_wlr_surface_at(node, lx, ly, &snode, sx, sy);

    if (snode == NULL || *surface == NULL)
        return NULL;
        
    //keep going up the tree, until our parent is one of the layers of the tree (2 ancenstors), 
    //which means we'll have found the top window
    while (snode != NULL && snode->parent != NULL && snode->parent->node.parent != NULL && snode->parent->node.parent->node.parent != NULL)
        snode = &snode->parent->node;

    if (snode == NULL)
        return NULL;

    //TODO: currently always a e_window, but soon could also be something related to a layer shell surface. This function will need to account for that!
    //We could get the root surface of the found surface and try getting the window from that surface?
    return snode->data; //struct e_window*
}

void e_window_send_close(struct e_window *window)
{
    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            wlr_xdg_toplevel_send_close(window->toplevel_window->xdg_toplevel);
            break;
        default:
            e_log_error("Can't request to close window, window is an unsupported type!");
            break;
    }
}

void e_window_destroy(struct e_window *window)
{
    if (window->scene_tree != NULL)
        wlr_scene_node_destroy(&window->scene_tree->node);

    free(window);
}