#include "desktop/views/view.h"

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
#include "desktop/views/window.h"

#include "input/seat.h"
#include "input/cursor.h"

#include "util/log.h"

#include "output.h"

//this function should only be called by the implementations of each view type. 
//I mean it would be a bit weird to even call this function somewhere else.
void e_view_init(struct e_view* view, struct e_desktop* desktop, enum e_view_type type, void* data)
{
    assert(view && desktop);

    view->desktop = desktop;
    view->type = type;
    view->data = data;

    view->tree = NULL;

    view->container = NULL;
    view->surface = NULL;

    view->current.x = -1;
    view->current.y = -1;
    view->current.width = -1;
    view->current.height = -1;

    view->title = NULL;
    view->tiled = false;

    view->implementation.set_tiled = NULL;
    view->implementation.configure = NULL;
    view->implementation.send_close = NULL;

    wl_signal_init(&view->events.set_title);

    wl_list_init(&view->link);
}

void e_view_set_position(struct e_view* view, int lx, int ly)
{
    assert(view && view->tree);

    e_view_configure(view, lx, ly, view->current.width, view->current.height);
}

uint32_t e_view_set_size(struct e_view* view, int width, int height)
{
    assert(view && view->tree);

    return e_view_configure(view, view->tree->node.x, view->tree->node.y, width, height);
}

void e_view_set_tiled(struct e_view* view, bool tiled)
{
    assert(view);

    e_log_info("setting tiling mode of view to %d...", tiled);

    if (view->tiled == tiled)
        return;

    view->tiled = tiled;

    if (view->implementation.set_tiled != NULL)
        view->implementation.set_tiled(view, tiled);
}

uint32_t e_view_configure(struct e_view* view, int lx, int ly, int width, int height)
{
    assert(view);

    #if E_VERBOSE
    e_log_info("view configure");
    #endif

    if (view->implementation.configure != NULL)
        return view->implementation.configure(view, lx, ly, width, height);
    else
        e_log_error("e_view_configure: configure is not implemented!");

    return 0;
}

void e_view_maximize(struct e_view* view)
{
    assert(view);

    //can't maximize tiled views
    if (!view->tiled && view->container != NULL)
        e_window_maximize(view->container);
}

static void e_view_create_container(struct e_view* view)
{
    assert(view && view->container == NULL);

    #if E_VERBOSE
    e_log_info("view create container tree");
    #endif

    //create container for view
    view->container = e_window_create(view);
}

void e_view_map(struct e_view* view)
{
    assert(view);

    e_log_info("map");

    wl_list_insert(&view->desktop->views, &view->link);

    e_view_create_container(view);

    //set focus to this view's main surface
    if (view->surface != NULL)
        e_seat_set_focus(view->desktop->seat, view->surface, false);
}

static void e_view_destroy_container(struct e_view* view)
{
    assert(view && view->container);

    struct e_tree_container* parent = view->container->base.parent;
    
    e_window_destroy(view->container);
    view->container = NULL;

    if (parent != NULL)
        e_tree_container_arrange(parent);
}

void e_view_unmap(struct e_view* view)
{   
    assert(view);

    struct e_seat* seat = view->desktop->seat;

    //if this view's surface had focus, clear it
    if (view->surface != NULL && e_seat_has_focus(seat, view->surface))
        e_seat_clear_focus(seat);

    //if this view's window container was grabbed by the cursor make it let go
    if (seat->cursor->grab_window == view->container)
        e_cursor_reset_mode(seat->cursor);

    #if E_VERBOSE
    e_log_info("view unmap");
    #endif

    if (view->container != NULL)
        e_view_destroy_container(view);

    wl_list_remove(&view->link);
        
    e_cursor_update_focus(seat->cursor);
}

struct e_view* e_view_from_surface(struct e_desktop* desktop, struct wlr_surface* surface)
{
    assert(desktop);
    
    if (surface == NULL)
        return NULL;

    if (wl_list_empty(&desktop->views))
        return NULL;

    struct e_view* view;

    wl_list_for_each(view, &desktop->views, link)
    {
        if (view->surface == NULL)
            continue;
        
        //found view with given surface as main surface
        if (view->surface == surface)
            return view;
    }

    //none found
    return NULL; 
}

// Returns NULL on fail.
static struct e_view* e_view_try_from_node(struct wlr_scene_node* node)
{
    if (node == NULL)
        return NULL;

    //data is either NULL or e_node_desc
    if (node->data == NULL)
        return NULL;

    struct e_node_desc* node_desc = node->data;

    if (node_desc->type == E_NODE_DESC_VIEW)
        return node_desc->data;
    else
        return NULL;
}

// Returns NULL on fail.
struct e_view* e_view_try_from_node_ancestors(struct wlr_scene_node* node)
{
    if (node == NULL)
        return NULL;

    struct e_view* view = e_view_try_from_node(node);

    if (view != NULL)
        return view;

    //keep going upwards in the tree until we find a view (in which case we return it), or reach the root of the tree (no parent)
    while (node->parent != NULL)
    {
        //go to parent node
        node = &node->parent->node;

        struct e_view* view = e_view_try_from_node(node);

        if (view != NULL)
            return view;
    }

    return NULL;
}

struct e_view* e_view_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy)
{
    if (node == NULL || surface == NULL || sx == NULL || sy == NULL)
    {
        e_log_error("e_view_at: *node, **surface, *sx, or *sy is NULL");
        abort();
    }

    struct wlr_scene_node* snode;
    *surface = e_desktop_wlr_surface_at(node, lx, ly, &snode, sx, sy);

    if (snode == NULL || *surface == NULL)
        return NULL;

    return e_view_try_from_node_ancestors(snode);
}

void e_view_send_close(struct e_view* view)
{
    assert(view);

    if (view->implementation.send_close != NULL)
        view->implementation.send_close(view);
    else
        e_log_error("e_view_send_close: send_close is not implemented!");
}

void e_view_fini(struct e_view* view)
{
    if (view->container != NULL)
        e_view_destroy_container(view);
    
    if (view->tree != NULL)
        wlr_scene_node_destroy(&view->tree->node);

    wl_list_init(&view->link);
    wl_list_remove(&view->link);
}