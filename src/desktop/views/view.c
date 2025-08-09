#include "desktop/views/view.h"

#include <stdint.h>
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
#include "desktop/tree/workspace.h"

#include "input/seat.h"

#include "util/log.h"

//this function should only be called by the implementations of each view type. 
//I mean it would be a bit weird to even call this function somewhere else.
void e_view_init(struct e_view* view, enum e_view_type type, void* data, const struct e_view_impl* implementation, struct wlr_scene_tree* parent)
{
    assert(view && implementation && parent);

    view->type = type;
    view->data = data;

    view->mapped = false;
    view->surface = NULL;

    view->tiled = false;
    view->fullscreen = false;

    view->title = NULL;
    
    view->tree = wlr_scene_tree_create(parent);
    e_node_desc_create(&view->tree->node, E_NODE_DESC_VIEW, view);

    view->root_geometry = (struct wlr_box){0, 0, 0, 0};
    view->width = 0;
    view->height = 0;

    view->content_tree = NULL;

    view->implementation = implementation;

    view->output = NULL;

    // signals

    wl_signal_init(&view->events.map);
    wl_signal_init(&view->events.unmap);

    wl_signal_init(&view->events.commit);

    wl_signal_init(&view->events.request_fullscreen);
    wl_signal_init(&view->events.request_move);
    wl_signal_init(&view->events.request_resize);
    wl_signal_init(&view->events.request_configure);

    wl_signal_init(&view->events.destroy);
}

// Returns size hints of view.
struct e_view_size_hints e_view_get_size_hints(struct e_view* view)
{
    if (view == NULL)
    {
        e_log_error("e_view_get_constraints: view is NULL!");
        return (struct e_view_size_hints){0};
    }

    if (view->implementation->get_size_hints != NULL)
    {
        return view->implementation->get_size_hints(view);
    }
    else
    {
        e_log_error("e_view_get_constraints: get_constraints is unimplemented!");
        return (struct e_view_size_hints){0};
    }
}

// Sets output of view.
// Output is allowed to be NULL.
void e_view_set_output(struct e_view* view, struct e_output* output)
{
    assert(view);

    if (view == NULL)
    {
        e_log_error("e_view_set_output: view is NULL!");
        return;
    }

    view->output = output;

    //TODO: later, foreign toplevel output enter & leave
}

// Set space for popups relative to view.
// Note: not relative to root toplevel surface, but to toplevels (0, 0) point. So no need to access view's geometry when setting this.
void e_view_set_popup_space(struct e_view* view, struct wlr_box popup_space)
{
    assert(view);

    if (view == NULL)
    {
        e_log_error("e_view_set_popup_space: view is NULL!");
        return;
    }

    view->popup_space = popup_space;
}

void e_view_configure(struct e_view* view, int lx, int ly, int width, int height)
{
    assert(view);

    #if E_VERBOSE
    e_log_info("view configure");
    #endif

    if (view->implementation->configure != NULL)
        view->implementation->configure(view, lx, ly, width, height);
    else
        e_log_error("e_view_configure: configure is not implemented!");
}

void e_view_set_tiled(struct e_view* view, bool tiled)
{
    assert(view);

    e_log_info("setting tiling mode of view to %d...", tiled);

    if (view->implementation->set_tiled != NULL)
        view->implementation->set_tiled(view, tiled);
    else
        e_log_error("e_view_set_tiled: not implemented!");
}

void e_view_set_activated(struct e_view* view, bool activated)
{
    assert(view);

    if (view->implementation->set_activated != NULL)
        view->implementation->set_activated(view, activated);
    else
        e_log_error("e_view_set_activated: not implemented!");
}

// Set the fullscreen mode of the view.
void e_view_set_fullscreen(struct e_view* view, bool fullscreen)
{
    assert(view);

    if (view->implementation->set_fullscreen != NULL)
        view->implementation->set_fullscreen(view, fullscreen);
    else
        e_log_error("e_view_set_fullscreen: set fullscreen not implemented!");
}

// Create a scene tree displaying this view's surfaces and subsurfaces.
// Returns NULL on fail.
static struct wlr_scene_tree* e_view_create_content_tree(struct e_view* view)
{
    assert(view);

    if (view->implementation->create_content_tree != NULL)
    {
        return view->implementation->create_content_tree(view);
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

    if (view->implementation->wants_floating != NULL)
    {
        return view->implementation->wants_floating(view);
    }
    else
    {
        e_log_error("e_view_wants_floating: not implemented!");
        return false;
    }
}

// Display view within view's tree & emit map signal.
// Set fullscreen to true and set fullscreen_output if you want to request that the view should be on a specific output immediately.
// Output is allowed to be NULL.
void e_view_map(struct e_view* view, bool fullscreen, struct e_output* fullscreen_output)
{
    assert(view);

    e_log_info("view map");

    view->content_tree = e_view_create_content_tree(view);

    if (view->content_tree == NULL)
    {
        e_log_error("e_view_map: unable to map view, failed to create content tree!");
        return;
    }

    view->mapped = true;

    struct e_view_map_event view_event = {
        .fullscreen = fullscreen,
        .fullscreen_output = fullscreen_output,
        .wants_floating = e_view_wants_floating(view)
    };

    wl_signal_emit_mutable(&view->events.map, &view_event);
}

// Stop displaying view within view's tree & emit unmap signal.
void e_view_unmap(struct e_view* view)
{   
    assert(view);

    #if E_VERBOSE
    e_log_info("view unmap");
    #endif

    view->mapped = false;

    if (view->content_tree != NULL)
    {
        wlr_scene_node_destroy(&view->content_tree->node);
        view->content_tree = NULL;
    }

    wl_signal_emit_mutable(&view->events.unmap, NULL);
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
    
    if (node == NULL)
        return NULL;

    struct wlr_scene_node* node_at = wlr_scene_node_at(node, lx, ly, NULL, NULL);
    
    if (node_at != NULL)
        return e_view_try_from_node_ancestors(node_at);
    else
        return NULL;
}

void e_view_send_close(struct e_view* view)
{
    assert(view);

    if (view->implementation->send_close != NULL)
        view->implementation->send_close(view);
    else
        e_log_error("e_view_send_close: send_close is not implemented!");
}

void e_view_fini(struct e_view* view)
{
    if (view->mapped)
        e_view_unmap(view);

    wlr_scene_node_destroy(&view->tree->node);
}
