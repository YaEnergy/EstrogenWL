#include "desktop/views/view.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_compositor.h>

#include <wlr/util/edges.h>
#include <wlr/util/box.h>

#include "desktop/desktop.h"
#include "desktop/output.h"
#include "desktop/tree/node.h"
#include "desktop/tree/container.h"

#include "input/seat.h"

#include "util/log.h"

static void e_container_configure_view(struct e_container* container, int lx, int ly, int width, int height)
{
    assert(container);

    struct e_view* view = container->data;

    e_view_configure(view, lx, ly, width, height);
}

static void e_container_destroy_view(struct e_container* container)
{
    assert(container);

    struct e_view* view = container->data;

    //TODO: doesn't destroy toplevel or xwayland data
    e_view_fini(view);
}

//this function should only be called by the implementations of each view type. 
//I mean it would be a bit weird to even call this function somewhere else.
void e_view_init(struct e_view* view, struct e_desktop* desktop, enum e_view_type type, void* data)
{
    assert(view && desktop);

    view->desktop = desktop;
    view->type = type;
    view->data = data;

    view->mapped = false;
    view->surface = NULL;

    view->title = NULL;
    
    view->tree = wlr_scene_tree_create(desktop->pending);
    wlr_scene_node_set_enabled(&view->tree->node, false);
    e_node_desc_create(&view->tree->node, E_NODE_DESC_VIEW, view);

    view->content_tree = NULL;

    view->implementation.set_tiled = NULL;
    view->implementation.set_activated = NULL;
    view->implementation.configure = NULL;
    view->implementation.create_content_tree = NULL;
    view->implementation.wants_floating = NULL;
    view->implementation.send_close = NULL;

    e_container_init(&view->container, E_CONTAINER_VIEW, view);
    view->container.implementation.configure = e_container_configure_view;
    view->container.implementation.destroy = e_container_destroy_view;

    wl_list_init(&view->link);
}

// Returns size hints of view.
struct e_view_size_hints e_view_get_size_hints(struct e_view* view)
{
    if (view == NULL)
    {
        e_log_error("e_view_get_constraints: view is NULL!");
        return (struct e_view_size_hints){0};
    }

    if (view->implementation.get_size_hints != NULL)
    {
        return view->implementation.get_size_hints(view);
    }
    else
    {
        e_log_error("e_view_get_constraints: get_constraints is unimplemented!");
        return (struct e_view_size_hints){0};
    }
}

static void e_view_set_parent_container(struct e_view* view, struct e_tree_container* parent)
{
    assert(view && parent);

    struct e_tree_container* previous_parent = view->container.parent;

    //parent container remains unchanged
    if (parent == previous_parent)
    {
        e_log_info("window parent remains unchanged");
        return;
    }

    #if E_VERBOSE
    e_log_info("setting window parent...");
    #endif

    //remove from & rearrange previous container
    if (previous_parent != NULL)
    {
        e_tree_container_remove_container(previous_parent, &view->container);
        e_tree_container_arrange(previous_parent);
    }

    e_tree_container_add_container(parent, &view->container);
    e_tree_container_arrange(parent);
}

static void e_view_tile(struct e_view* view)
{
    assert(view);

    #if E_VERBOSE
    e_log_info("view tile container");
    #endif

    //if not focused, find current TILED focused view's parent container
    // else if focused or if none, find previous TILED focused's parent container
    //if none, then output 0's root tiling container
    //add to container if found
    //else give up

    struct e_desktop* desktop = view->desktop;
    struct e_seat* seat = desktop->seat;

    struct wlr_surface* view_surface = view->surface;

    struct e_tree_container* parent_container = NULL;

    if (view_surface != NULL)
    {
        //if not focused, find current TILED focused view's parent container
        if (!e_seat_has_focus(seat, view_surface) && seat->focus_surface != NULL)
        {
            struct e_view* focused_view = e_seat_focused_view(seat);

            if (focused_view != NULL && focused_view->tiled && focused_view->container.parent != NULL)
                parent_container = focused_view->container.parent;
        }

        //if focused or none, find previous TILED focused view's parent container
        if ((parent_container == NULL || e_seat_has_focus(seat, view_surface)) && seat->previous_focus_surface != NULL)
        {
            struct e_view* prev_focused_view = e_seat_prev_focused_view(seat);

            if (prev_focused_view != NULL && prev_focused_view->tiled && prev_focused_view->container.parent != NULL)
                parent_container = prev_focused_view->container.parent;
        }
    }
    
    //if still none, then output 0's root tiling container
    if (parent_container == NULL)
    {
        //Set to output 0's container
        struct e_output* output = e_desktop_get_output(desktop, 0);

        if (output != NULL && output->root_tiling_container != NULL)
            parent_container = output->root_tiling_container;
    }

    if (parent_container == NULL)
    {
        e_log_error("Failed to set view's parent to a tiled container");
        return;
    }
    
    e_view_set_parent_container(view, parent_container);

    wlr_scene_node_reparent(&view->tree->node, view->desktop->layers.tiling);
}

// Returns the view's first current output with a root floating container
// May return NULL.
static struct e_tree_container* e_view_first_output_floating_container(struct e_view* view)
{
    assert(view);

    if (view->surface == NULL)
        return NULL;

    if (wl_list_empty(&view->surface->current_outputs))
        return NULL;

    //loop over surface outputs until output is found with a root floating container
    struct wlr_surface_output* wlr_surface_output;
    wl_list_for_each(wlr_surface_output, &view->surface->current_outputs, link)
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

static void e_view_float(struct e_view* view)
{
    assert(view);

    #if E_VERBOSE
    e_log_info("view float container");
    #endif

    //find first of current outputs root floating container
    //if none, then output 0's root floating container
    //add to container if found
    //else give up

    struct wlr_surface* view_surface = view->surface;

    struct e_tree_container* parent_container = NULL;

    if (view_surface != NULL)
        parent_container = e_view_first_output_floating_container(view);

    if (parent_container == NULL)
    {
        //Set to output 0's container
        struct e_output* output = e_desktop_get_output(view->desktop, 0);

        if (output != NULL && output->root_floating_container != NULL)
            parent_container = output->root_floating_container;
    }

    if (parent_container == NULL)
    {
        e_log_error("Failed to set window's parent to a floating container");
        return;
    }

    e_view_set_parent_container(view, parent_container);

    wlr_scene_node_reparent(&view->tree->node, view->desktop->layers.floating);

    //TODO: return to natural geometry
}

// Set pending layout position of view.
void e_view_set_position(struct e_view* view, int lx, int ly)
{
    assert(view);

    e_view_configure(view, lx, ly, view->pending.width, view->pending.height);
}

void e_view_configure(struct e_view* view, int lx, int ly, int width, int height)
{
    assert(view);

    #if E_VERBOSE
    e_log_info("view configure");
    #endif

    if (view->implementation.configure != NULL)
        view->implementation.configure(view, lx, ly, width, height);
    else
        e_log_error("e_view_configure: configure is not implemented!");
}

void e_view_set_tiled(struct e_view* view, bool tiled)
{
    assert(view);

    e_log_info("setting tiling mode of view to %d...", tiled);

    view->tiled = tiled;

    if (tiled)
        e_view_tile(view);
    else
        e_view_float(view);

    if (view->implementation.set_tiled != NULL)
        view->implementation.set_tiled(view, tiled);
}

void e_view_set_activated(struct e_view* view, bool activated)
{
    assert(view);

    if (view->implementation.set_activated != NULL)
        view->implementation.set_activated(view, activated);
    else
        e_log_error("e_view_set_activated: not implemented!");
}

bool e_view_has_pending_changes(struct e_view* view)
{
    return !wlr_box_equal(&view->current, &view->pending) && !wlr_box_empty(&view->pending);
}

// Configure view using pending changes.
void e_view_configure_pending(struct e_view* view)
{
    assert(view);

    #if E_VERBOSE
    e_log_info("view configure pending");
    #endif

    e_view_configure(view, view->pending.x, view->pending.y, view->pending.width, view->pending.height);
}

// Updates view's tree node to current position.
// Should be called when view has moved. (Current x & y changed)
void e_view_moved(struct e_view* view)
{
    assert(view);

    wlr_scene_node_set_position(&view->tree->node, view->current.x, view->current.y);
}

// Create a scene tree displaying this view's surfaces and subsurfaces.
// Returns NULL on fail.
static struct wlr_scene_tree* e_view_create_content_tree(struct e_view* view)
{
    assert(view);

    if (view->implementation.create_content_tree != NULL)
    {
        return view->implementation.create_content_tree(view);
    }
    else
    {
        e_log_error("e_view_create_content_tree: create content tree not implemented!");
        return NULL;
    }
}

static bool e_view_wants_floating(struct e_view* view)
{
    assert(view);

    if (view->implementation.wants_floating != NULL)
    {
        return view->implementation.wants_floating(view);
    }
    else
    {
        e_log_error("e_view_wants_floating: not implemented!");
        return false;
    }
}

// Display view.
void e_view_map(struct e_view* view)
{
    assert(view);

    e_log_info("view map");

    wl_list_insert(&view->desktop->views, &view->link);

    view->content_tree = e_view_create_content_tree(view);

    if (view->content_tree == NULL)
    {
        e_log_error("e_view_map: unable to map view, failed to create content tree!");
        return;
    }

    view->mapped = true;

    bool wants_floating = e_view_wants_floating(view);

    e_view_set_tiled(view, !wants_floating);

    wlr_scene_node_set_position(&view->tree->node, view->current.x, view->current.y);

    wlr_scene_node_set_enabled(&view->tree->node, true);

    //set focus to this view
    if (view->surface != NULL)
        e_seat_set_focus_view(view->desktop->seat, view);
}

// Stop displaying view.
void e_view_unmap(struct e_view* view)
{   
    assert(view);

    #if E_VERBOSE
    e_log_info("view unmap");
    #endif

    view->mapped = false;
    wlr_scene_node_set_enabled(&view->tree->node, false);
    wlr_scene_node_destroy(&view->content_tree->node);
    view->content_tree = NULL;

    if (view->container.parent != NULL)
    {
        struct e_tree_container* parent = view->container.parent;
    
        e_tree_container_remove_container(parent, &view->container);
        e_tree_container_arrange(parent);
    }

    wl_list_remove(&view->link);
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

// Finds the view at the specified layout coords in given scene graph.
// Returns NULL if nothing is found.
struct e_view* e_view_at(struct wlr_scene_node* node, double lx, double ly)
{
    assert(node);
    
    struct wlr_scene_surface* scene_surface = e_desktop_scene_surface_at(node, lx, ly, NULL, NULL);

    if (scene_surface == NULL)
        return NULL;

    return e_view_try_from_node_ancestors(&scene_surface->buffer->node);
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
    if (view->mapped)
        e_view_unmap(view);

    wlr_scene_node_destroy(&view->tree->node);
}
