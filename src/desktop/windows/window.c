#include "desktop/windows/window.h"

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_compositor.h>

#include <wlr/util/edges.h>
#include <wlr/util/box.h>

#include "desktop/scene.h"
#include "desktop/xwayland.h"

#include "desktop/windows/toplevel_window.h"
#include "desktop/windows/xwayland_window.h"

#include "desktop/tree/container.h"
#include "desktop/tree/node.h"

#include "input/input_manager.h"
#include "input/seat.h"
#include "input/cursor.h"

#include "util/log.h"

#include "output.h"
#include "server.h"

//this function should only be called by the implementations of each window type. 
//I mean it would be a bit weird to even call this function somewhere else.
struct e_window* e_window_create(struct e_server* server, enum e_window_type type)
{
    struct e_window* window = calloc(1, sizeof(struct e_window));
    window->server = server;
    window->type = type;

    window->scene_tree = NULL;
    window->container = NULL;

    wl_list_init(&window->link);

    return window;
}

static void e_window_create_window_tree(struct e_window* window, struct wlr_scene_tree* parent)
{
    assert(window && parent && window->scene_tree == NULL);

    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            //create scene xdg surface for xdg toplevel and window, and set up window scene tree
            window->scene_tree = wlr_scene_xdg_surface_create(parent, window->toplevel_window->xdg_toplevel->base);
            break;
        case E_WINDOW_XWAYLAND:
            //add surface & subsurfaces to scene by creating a subsurface tree
            window->scene_tree = wlr_scene_subsurface_tree_create(parent, window->xwayland_window->xwayland_surface->surface);
            break;
        default:
            e_log_error("Can't add window to scene, window is an unsupported type!");
            return;
    }

    if (window->scene_tree != NULL)
    {
        //allows retrieving of window data from the tree, neccessary for e_window_at
        e_node_desc_create(&window->scene_tree->node, E_NODE_DESC_WINDOW, window);
    }
}

void e_window_create_container_tree(struct e_window* window, struct wlr_scene_tree* parent)
{
    assert(window && parent && window->container == NULL);

    e_window_create_window_tree(window, parent);

    //create container for window
    window->container = e_container_window_create(parent, window);

    if (parent->node.data != NULL) //struct e_node_desc*
    {
        struct e_container* parent_container = e_container_try_from_e_node_desc(parent->node.data);

        if (parent_container != NULL)
        {
            e_container_set_parent(window->container, parent_container);
            e_container_arrange(parent_container);
        }
    }
}

void e_window_destroy_container_tree(struct e_window* window)
{
    assert(window);

    if (window->scene_tree != NULL)
    {
        wlr_scene_node_destroy(&window->scene_tree->node);
        window->scene_tree = NULL;
    }
    
    if (window->container != NULL)
    {
        e_container_destroy(window->container);
        window->container = NULL;
    }
}

char* e_window_get_title(struct e_window* window)
{
    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            return window->toplevel_window->xdg_toplevel->title;
        case E_WINDOW_XWAYLAND:
            return window->xwayland_window->xwayland_surface->title;
        default:
            e_log_error("Can't get title of window, window is an unsupported type!");
            return NULL;
    }
}

//outs top left x and y of window relative to parent node
void e_window_get_position(struct e_window* window, int* lx, int* ly)
{
    if (lx == NULL || ly == NULL)
    {
        e_log_error("e_window_get_position: please give valid pointers");
        abort();
    }

    if (window->scene_tree == NULL)
    {
        e_log_error("e_window_get_position: window has no scene tree");
        abort();
    }

    *lx = window->scene_tree->node.x;
    *ly = window->scene_tree->node.y;
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
        case E_WINDOW_XWAYLAND:
            *x = (int)window->xwayland_window->xwayland_surface->width;
            *y = (int)window->xwayland_window->xwayland_surface->height;
            break;
        default:
            *x = -1;
            *y = -1;
            e_log_error("Can't get size of window, window is an unsupported type!");
            break;
    }
}

void e_window_set_position(struct e_window* window, int lx, int ly)
{
    if (window->scene_tree == NULL)
    {
        e_log_error("Can't set position of window, window has no scene tree!");
        return;
    }

    wlr_scene_node_set_position(&window->scene_tree->node, lx, ly);

    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            //do nothing
            break;
        case E_WINDOW_XWAYLAND:
        {
            //get position of this window's node relative to root node
            int x = window->scene_tree->node.x;
            int y = window->scene_tree->node.y;

            struct wlr_scene_tree* tree = window->scene_tree;
            while (tree->node.parent != NULL)
            {
                tree = tree->node.parent;
                x += tree->node.x;
                y += tree->node.y;
            }

            wlr_xwayland_surface_configure(window->xwayland_window->xwayland_surface, x, y, window->xwayland_window->xwayland_surface->width, window->xwayland_window->xwayland_surface->height);
            break;
        }
    }
}

void e_window_set_size(struct e_window* window, int32_t x, int32_t y)
{
    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
                        wlr_xdg_toplevel_set_size(window->toplevel_window->xdg_toplevel, x, y);
            break;
        case E_WINDOW_XWAYLAND:
                        wlr_xwayland_surface_configure(window->xwayland_window->xwayland_surface, window->xwayland_window->xwayland_surface->x, window->xwayland_window->xwayland_surface->y, x, y);
            break;
        default:
            e_log_error("Can't set size of window, window is an unsupported type!");
            break;
    }
}

static void e_window_set_parent_container(struct e_window* window, struct e_container* parent)
{
    assert(window && parent);

    struct e_container* previous_parent = window->container->parent;

    //parent container remains unchanged
    if (parent == previous_parent)
    {
        e_log_info("parent container remains unchanged");
        return;
    }

    //remove from & rearrange previous container
    if (previous_parent != NULL)
    {
        e_container_remove_container(previous_parent, window->container);
        e_container_arrange(previous_parent);
    }

    e_container_set_parent(window->container, parent);
    e_container_arrange(parent);
}

static void e_window_tile_container(struct e_window* window)
{
    assert(window && window->container);

    //if not focused, find current TILED focused window's parent container
    // else if focused or if none, find previous TILED focused's parent container
    //if none, then output 0's root tiling container
    //add to container if found
    //else give up

    struct e_server* server = window->server;
    struct e_seat* seat = server->input_manager->seat;

    struct wlr_surface* window_surface = e_window_get_surface(window);

    struct e_container* parent_container = NULL;

    if (window_surface != NULL)
    {
        //if not focused, find current TILED focused window's parent container
        if (!e_seat_has_focus(seat, window_surface) && seat->focus_surface != NULL)
        {
            struct e_window* focus_window = e_window_from_surface(server, seat->focus_surface);

            if (focus_window != NULL && focus_window->tiled && focus_window->container != NULL)
                parent_container = focus_window->container->parent;
        }

        //if focused or none, find previous TILED focused's parent container
        if ((parent_container == NULL || e_seat_has_focus(seat, window_surface)) && seat->previous_focus_surface != NULL)
        {
            struct e_window* previous_focus_window = e_window_from_surface(server, seat->previous_focus_surface);

            if (previous_focus_window != NULL && previous_focus_window->tiled && previous_focus_window->container != NULL)
                parent_container = previous_focus_window->container->parent;
        }
    }
    
    //if still none, then output 0's root tiling container
    if (parent_container == NULL)
    {
        //Set to output 0's container
        struct e_output* output = e_scene_get_output(window->server->scene, 0);

        if (output != NULL && output->root_tiling_container != NULL)
            parent_container = output->root_tiling_container;
    }

    if (parent_container == NULL)
    {
        e_log_error("Failed to set window container's parent to a tiled container");
        return;
    }
    
    e_window_set_parent_container(window, parent_container);
}

static void e_window_float_container(struct e_window* window)
{
    assert(window && window->container);

    //find first of current outputs root floating container
    //if none, then output 0's root floating container
    //add to container if found
    //else give up

    struct wlr_surface* window_surface = e_window_get_surface(window);

    struct e_container* parent_container = NULL;

    if (window_surface != NULL)
    {
        //loop over surface outputs until output is found with a root floating container
        struct wlr_surface_output* wlr_surface_output;
        wl_list_for_each(wlr_surface_output, &window_surface->current_outputs, link)
        {
            //attempt to acquire output from it
            if (wlr_surface_output->output != NULL && wlr_surface_output->output->data != NULL)
            {
                struct e_output* output = wlr_surface_output->output->data;

                if (output->root_floating_container != NULL)
                {
                    parent_container = output->root_floating_container;
                    e_log_info("output found!");
                    break;
                }
            }
        }
    }

    if (parent_container == NULL)
    {
        //Set to output 0's container
        struct e_output* output = e_scene_get_output(window->server->scene, 0);

        if (output != NULL && output->root_floating_container != NULL)
            parent_container = output->root_floating_container;
    }

    if (parent_container == NULL)
    {
        e_log_error("Failed to set window container's parent to a floating container");
        return;
    }

    e_window_set_parent_container(window, parent_container);
}

void e_window_set_tiled(struct e_window* window, bool tiled)
{
    assert(window);

    e_log_info("setting tiling mode of window to %B...", tiled);

    if (window->tiled == tiled)
        return;

    window->tiled = tiled;

    if (window->container != NULL)
    {
        //Reparent to current focused container
        if (window->tiled)
            e_window_tile_container(window);
        else //floating
            e_window_float_container(window);
    }

    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            wlr_xdg_toplevel_set_tiled(window->toplevel_window->xdg_toplevel, tiled ? WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP : WLR_EDGE_NONE);
            break;
        case E_WINDOW_XWAYLAND:
            //do nothing
            break;
        default:
            e_log_error("Can't properly set tiled mode of window, window is an unsupported type!");
            break;
    }
}

uint32_t e_window_configure(struct e_window* window, int lx, int ly, int width, int height)
{
    assert(window && window->scene_tree);

    wlr_scene_node_set_position(&window->scene_tree->node, lx, ly);

    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            return wlr_xdg_toplevel_set_size(window->toplevel_window->xdg_toplevel, width, height);
        case E_WINDOW_XWAYLAND:
            wlr_xwayland_surface_configure(window->xwayland_window->xwayland_surface, lx, ly, (uint16_t)width, (uint16_t)height);
            return 0;
        default:
            e_log_error("Can't set size of window, window is an unsupported type!");
            return 0;
    }
}

void e_window_map(struct e_window* window)
{
    e_log_info("map");

    wl_list_insert(&window->server->scene->windows, &window->link);

    if (window->container->parent == NULL)
    {
        if (window->tiled)
            e_window_tile_container(window);
        else //floating
            e_window_float_container(window);
    }

    struct wlr_surface* window_surface = e_window_get_surface(window);

    //set focus to this window's main surface
    if (window_surface != NULL)
        e_seat_set_focus(window->server->input_manager->seat, window_surface, false);
}

void e_window_unmap(struct e_window* window)
{   
    struct e_input_manager* input_manager = window->server->input_manager;

    struct wlr_surface* window_surface = e_window_get_surface(window);

    //if this window's surface had focus, clear it
    if (window_surface != NULL && e_seat_has_focus(input_manager->seat, window_surface))
        e_seat_clear_focus(input_manager->seat);

    //if this window was grabbed by the cursor make it let go
    if (input_manager->cursor->grab_window == window)
        e_cursor_reset_mode(input_manager->cursor);

    struct e_container* parent_container = window->container->parent;
    if (parent_container != NULL)
    {
        e_container_remove_container(parent_container, window->container);
        e_container_arrange(parent_container);
    }

    wl_list_remove(&window->link);
        
    e_cursor_update_focus(input_manager->cursor);
}

struct wlr_surface* e_window_get_surface(struct e_window* window)
{
    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            return window->toplevel_window->xdg_toplevel->base->surface;
        case E_WINDOW_XWAYLAND:
            //I'm unsure if this just points to NULL if invalid...
            return window->xwayland_window->xwayland_surface->surface;
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

//may return NULL
struct e_window* e_window_try_from_node_ancestors(struct wlr_scene_node* node)
{
    if (node == NULL)
        return NULL;

    //keep going upwards in the tree until we find a window (in which case we return it), or reach the root of the tree (no parent)
    while (node->parent != NULL)
    {
        //data is either NULL or e_node_desc
        if (node->data != NULL)
        {
            struct e_node_desc* node_desc = node->data;

            if (node_desc->type == E_NODE_DESC_WINDOW)
                return node_desc->data;
        }

        //go to parent node
        node = &node->parent->node;
    }

    return NULL;
}

struct e_window* e_window_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy)
{
    if (node == NULL || surface == NULL || sx == NULL || sy == NULL)
    {
        e_log_error("e_window_at: *node, **surface, *sx, or *sy is NULL");
        abort();
    }

    struct wlr_scene_node* snode;
    *surface = e_scene_wlr_surface_at(node, lx, ly, &snode, sx, sy);

    if (snode == NULL || *surface == NULL)
        return NULL;

    return e_window_try_from_node_ancestors(snode);
}

void e_window_send_close(struct e_window *window)
{
    switch(window->type)
    {
        case E_WINDOW_TOPLEVEL:
            wlr_xdg_toplevel_send_close(window->toplevel_window->xdg_toplevel);
            break;
        case E_WINDOW_XWAYLAND:
            wlr_xwayland_surface_close(window->xwayland_window->xwayland_surface);
            break;
        default:
            e_log_error("Can't request to close window, window is an unsupported type!");
            break;
    }
}

void e_window_destroy(struct e_window *window)
{
    e_window_destroy_container_tree(window);

    free(window);
}