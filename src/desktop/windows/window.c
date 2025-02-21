#include "desktop/windows/window.h"

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_compositor.h>

#include <wlr/util/edges.h>
#include <wlr/util/box.h>

#include "desktop/scene.h"

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
    struct e_window* window = calloc(1, sizeof(*window));
    window->server = server;
    window->type = type;

    window->tree = NULL;
    window->container = NULL;
    window->surface = NULL;

    window->current.x = -1;
    window->current.y = -1;
    window->current.width = -1;
    window->current.height = -1;

    window->title = NULL;
    window->tiled = false;

    window->implementation.changed_tiled = NULL;
    window->implementation.configure = NULL;
    window->implementation.send_close = NULL;

    wl_list_init(&window->link);

    return window;
}

void e_window_create_container_tree(struct e_window* window, struct wlr_scene_tree* parent)
{
    assert(window && parent && window->container == NULL);

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
    
    if (window->container != NULL)
    {
        e_container_destroy(window->container);
        window->container = NULL;
        window->tree = NULL;
    }
    else if (window->tree != NULL)
    {
        wlr_scene_node_destroy(&window->tree->node);
        window->tree = NULL;
    }
}

void e_window_set_position(struct e_window* window, int lx, int ly)
{
    assert(window && window->tree);

    e_window_configure(window, lx, ly, window->current.width, window->current.height);
}

uint32_t e_window_set_size(struct e_window* window, int width, int height)
{
    assert(window && window->tree);

    return e_window_configure(window, window->tree->node.x, window->tree->node.y, width, height);
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

    struct wlr_surface* window_surface = window->surface;

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

// Returns the window's first current output with a root floating container
// May return NULL.
static struct e_container* e_window_first_output_floating_container(struct e_window* window)
{
    assert(window);

    if (window->surface == NULL)
        return NULL;

    if (wl_list_empty(&window->surface->current_outputs))
        return NULL;

    //loop over surface outputs until output is found with a root floating container
    struct wlr_surface_output* wlr_surface_output;
    wl_list_for_each(wlr_surface_output, &window->surface->current_outputs, link)
    {
        //attempt to acquire output from it
        if (wlr_surface_output->output != NULL && wlr_surface_output->output->data != NULL)
        {
            struct e_output* output = wlr_surface_output->output->data;

            if (output->root_floating_container != NULL)
                return output->root_floating_container;
        }
    }

    //none found
    return NULL;
}

static void e_window_float_container(struct e_window* window)
{
    assert(window && window->container);

    //find first of current outputs root floating container
    //if none, then output 0's root floating container
    //add to container if found
    //else give up

    struct wlr_surface* window_surface = window->surface;

    struct e_container* parent_container = NULL;

    if (window_surface != NULL)
        parent_container = e_window_first_output_floating_container(window);

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

    e_log_info("setting tiling mode of window to %d...", tiled);

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

    if (window->implementation.changed_tiled != NULL)
        return window->implementation.changed_tiled(window, tiled);
}

uint32_t e_window_configure(struct e_window* window, int lx, int ly, int width, int height)
{
    assert(window);

    if (window->implementation.configure != NULL)
        return window->implementation.configure(window, lx, ly, width, height);
    else
        e_log_error("e_window_configure: configure is not implemented!");

    return 0;
}

void e_window_maximize(struct e_window* window)
{
    assert(window && window->container);

    //can't maximize tiled windows or window containers with no parent
    if (window->tiled || window->container->parent == NULL)
        return;

    e_container_configure(window->container, 0, 0, window->container->parent->area.width, window->container->parent->area.height);
}

void e_window_map(struct e_window* window)
{
    assert(window);

    e_log_info("map");

    wl_list_insert(&window->server->scene->windows, &window->link);

    if (window->container->parent == NULL)
    {
        if (window->tiled)
            e_window_tile_container(window);
        else //floating
            e_window_float_container(window);
    }

    //set focus to this window's main surface
    if (window->surface != NULL)
        e_seat_set_focus(window->server->input_manager->seat, window->surface, false);
}

void e_window_unmap(struct e_window* window)
{   
    assert(window);

    struct e_input_manager* input_manager = window->server->input_manager;

    //if this window's surface had focus, clear it
    if (window->surface != NULL && e_seat_has_focus(input_manager->seat, window->surface))
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

struct e_window* e_window_from_surface(struct e_server* server, struct wlr_surface* surface)
{
    if (wl_list_empty(&server->scene->windows))
        return NULL;

    struct e_window* window;

    wl_list_for_each(window, &server->scene->windows, link)
    {
        if (window->surface == NULL)
            continue;
        
        //found window with given surface as main surface
        if (window->surface == surface)
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

void e_window_send_close(struct e_window* window)
{
    assert(window);

    if (window->implementation.send_close != NULL)
        window->implementation.send_close(window);
    else
        e_log_error("e_window_send_close: send_close is not implemented!");
}

void e_window_destroy(struct e_window* window)
{
    if (window->container != NULL || window->tree != NULL)
        e_window_destroy_container_tree(window);

    wl_list_init(&window->link);
    wl_list_remove(&window->link);

    free(window);
}