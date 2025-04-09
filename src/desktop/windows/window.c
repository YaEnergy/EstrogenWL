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

#include "desktop/desktop.h"
#include "desktop/tree/node.h"

#include "desktop/tree/container.h"
#include "desktop/windows/window_container.h"

#include "input/seat.h"
#include "input/cursor.h"

#include "util/log.h"

#include "output.h"

//this function should only be called by the implementations of each window type. 
//I mean it would be a bit weird to even call this function somewhere else.
void e_window_init(struct e_window* window, struct e_desktop* desktop, enum e_window_type type, void* data)
{
    assert(window && desktop);

    window->desktop = desktop;
    window->type = type;
    window->data = data;

    window->tree = NULL;

    window->container = NULL;
    window->surface = NULL;

    window->current.x = -1;
    window->current.y = -1;
    window->current.width = -1;
    window->current.height = -1;

    window->title = NULL;
    window->tiled = false;

    window->implementation.set_tiled = NULL;
    window->implementation.configure = NULL;
    window->implementation.send_close = NULL;

    wl_signal_init(&window->events.set_tiled);
    wl_signal_init(&window->events.set_title);

    wl_list_init(&window->link);
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

void e_window_set_tiled(struct e_window* window, bool tiled)
{
    assert(window);

    e_log_info("setting tiling mode of window to %d...", tiled);

    if (window->tiled == tiled)
        return;

    window->tiled = tiled;

    if (window->implementation.set_tiled != NULL)
        window->implementation.set_tiled(window, tiled);

    wl_signal_emit(&window->events.set_tiled, NULL);
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
    assert(window);

    //can't maximize tiled windows
    if (!window->tiled && window->container != NULL)
        e_window_container_maximize(window->container);
}

static void e_window_create_container(struct e_window* window)
{
    assert(window && window->container == NULL);

    #if E_VERBOSE
    e_log_info("window create container tree");
    #endif

    //create container for window
    window->container = e_window_container_create(window);
}

void e_window_map(struct e_window* window)
{
    assert(window);

    e_log_info("map");

    wl_list_insert(&window->desktop->windows, &window->link);

    e_window_create_container(window);

    //set focus to this window's main surface
    if (window->surface != NULL)
        e_seat_set_focus(window->desktop->seat, window->surface, false);
}

static void e_window_destroy_container(struct e_window* window)
{
    assert(window && window->container);

    struct e_tree_container* parent = window->container->base.parent;
    
    //keep window content tree
    if (window->tree != NULL)
        wlr_scene_node_reparent(&window->tree->node, window->desktop->pending);

    e_window_container_destroy(window->container);
    window->container = NULL;

    if (parent != NULL)
        e_tree_container_arrange(parent);
}

void e_window_unmap(struct e_window* window)
{   
    assert(window);

    struct e_seat* seat = window->desktop->seat;

    //if this window's surface had focus, clear it
    if (window->surface != NULL && e_seat_has_focus(seat, window->surface))
        e_seat_clear_focus(seat);

    //if this window was grabbed by the cursor make it let go
    if (seat->cursor->grab_window == window)
        e_cursor_reset_mode(seat->cursor);

    #if E_VERBOSE
    e_log_info("window unmap");
    #endif

    if (window->container != NULL)
        e_window_destroy_container(window);

    wl_list_remove(&window->link);
        
    e_cursor_update_focus(seat->cursor);
}

struct e_window* e_window_from_surface(struct e_desktop* desktop, struct wlr_surface* surface)
{
    if (wl_list_empty(&desktop->windows))
        return NULL;

    struct e_window* window;

    wl_list_for_each(window, &desktop->windows, link)
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
    *surface = e_desktop_wlr_surface_at(node, lx, ly, &snode, sx, sy);

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

void e_window_fini(struct e_window* window)
{
    if (window->container != NULL)
        e_window_destroy_container(window);
    
    if (window->tree != NULL)
        wlr_scene_node_destroy(&window->tree->node);

    wl_list_init(&window->link);
    wl_list_remove(&window->link);
}