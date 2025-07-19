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

#include "util/list.h"
#include "util/log.h"

#include "server.h"

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
void e_view_init(struct e_view* view, struct e_server* server, enum e_view_type type, void* data, const struct e_view_impl* implementation)
{
    assert(view && server && implementation);

    view->server = server;
    view->type = type;
    view->data = data;

    view->mapped = false;
    view->surface = NULL;

    view->tiled = false;
    view->fullscreen = false;

    view->title = NULL;
    
    view->tree = wlr_scene_tree_create(server->pending);
    wlr_scene_node_set_enabled(&view->tree->node, false);
    e_node_desc_create(&view->tree->node, E_NODE_DESC_VIEW, view);

    view->content_tree = NULL;

    view->implementation = implementation;

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

// Parent is allowed to be NULL.
static void e_view_set_parent_container(struct e_view* view, struct e_tree_container* parent)
{
    assert(view);

    struct e_tree_container* previous_parent = view->container.parent;

    //parent container remains unchanged
    if (parent == previous_parent)
    {
        e_log_info("view parent remains unchanged");
        return;
    }

    #if E_VERBOSE
    e_log_info("setting view parent...");
    #endif

    //remove from & rearrange previous container
    if (previous_parent != NULL)
    {
        e_tree_container_remove_container(previous_parent, &view->container);
        e_tree_container_arrange(previous_parent);
    }
    
    if (parent != NULL)
    {
        e_tree_container_add_container(parent, &view->container);
        e_tree_container_arrange(parent);
    }
}

// Sets workspace of view, but doesn't set parent container of view.
// Workspace is allowed to be NULL.
static void e_view_set_workspace(struct e_view* view, struct e_workspace* workspace)
{
    if (view == NULL)
    {
        e_log_error("e_view_set_workspace: view is NULL!");
        return;
    }

    if (view->workspace != NULL)
    {
        //remove from old floating list if there

        int floating_index = e_list_find_index(&view->workspace->floating_views, view);

        if (floating_index != -1)
            e_list_remove_index(&view->workspace->floating_views, floating_index);

        //reenable workspace trees if workspace is active and view was fullscreen
        if (view->fullscreen)
        {
            view->workspace->fullscreen_view = NULL;
            e_workspace_update_tree_visibility(view->workspace);
        }
    }
    
    if (workspace != NULL)
    {
        if (view->fullscreen)
        {
            if (workspace->fullscreen_view != NULL && workspace->fullscreen_view != view)
                e_view_set_fullscreen(workspace->fullscreen_view, false);

            workspace->fullscreen_view = view;
            wlr_scene_node_reparent(&view->tree->node, workspace->layers.fullscreen);
            e_workspace_update_tree_visibility(workspace);
            e_workspace_arrange(workspace, workspace->full_area, workspace->tiled_area);
        }
        else if (view->tiled)
        {
            wlr_scene_node_reparent(&view->tree->node, workspace->layers.tiling);
        }
        else //floating 
        {
            wlr_scene_node_reparent(&view->tree->node, workspace->layers.floating);
            e_list_add(&workspace->floating_views, view);
        }
    }
    else 
    {
        wlr_scene_node_reparent(&view->tree->node, view->server->pending);
    }

    view->workspace = workspace;
}

// Moves view to a different workspace, and updating its container parent.
// If workspace is NULL, removes view from current workspace.
void e_view_move_to_workspace(struct e_view* view, struct e_workspace* workspace)
{
    if (view == NULL)
    {
        e_log_error("e_view_set_workspace: view is NULL!");
        return;
    }

    e_view_set_workspace(view, workspace);
    
    if (workspace != NULL && view->tiled && !view->fullscreen)
        e_view_set_parent_container(view, workspace->root_tiling_container);
    else //floating or NULL workspace or fullscreen
        e_view_set_parent_container(view, NULL);
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

    if (view->implementation->configure != NULL)
        view->implementation->configure(view, lx, ly, width, height);
    else
        e_log_error("e_view_configure: configure is not implemented!");
}

void e_view_set_tiled(struct e_view* view, bool tiled)
{
    assert(view);

    e_log_info("setting tiling mode of view to %d...", tiled);

    if (view->tiled != tiled)
    {
        view->tiled = tiled;

        if (view->implementation->notify_tiled != NULL)
            view->implementation->notify_tiled(view, tiled);
    }

    struct e_workspace* workspace = view->workspace;

    if (workspace != NULL && !view->fullscreen)
    {
        //TODO: tiled -> parent to previously focused tiled container
        //TODO: floating -> return to natural geometry and center

        e_view_set_parent_container(view, tiled ? workspace->root_tiling_container : NULL);
        wlr_scene_node_reparent(&view->tree->node, tiled ? workspace->layers.tiling : workspace->layers.floating);
    }
}

void e_view_set_activated(struct e_view* view, bool activated)
{
    assert(view);

    if (view->implementation->set_activated != NULL)
        view->implementation->set_activated(view, activated);
    else
        e_log_error("e_view_set_activated: not implemented!");
}


// Set view to fullscreen.
void e_view_fullscreen(struct e_view* view)
{
    assert(view);

    struct e_workspace* workspace = view->workspace;

    if (workspace == NULL)
    {
        e_log_error("e_view_unfullscreen: workspace is NULL!");
        return;
    }

    if (workspace->fullscreen_view == view)
        return;

    if (workspace->fullscreen_view != NULL)
        e_view_set_fullscreen(workspace->fullscreen_view, false);

    view->fullscreen = true;
    workspace->fullscreen_view = view;
    e_view_set_parent_container(view, NULL);

    wlr_scene_node_reparent(&view->tree->node, workspace->layers.fullscreen);

    e_workspace_update_tree_visibility(workspace);
    e_workspace_arrange(workspace, workspace->full_area, workspace->tiled_area);
}

// Unfullscreen view.
void e_view_unfullscreen(struct e_view* view)
{
    assert(view);

    struct e_workspace* workspace = view->workspace;

    if (workspace == NULL)
    {
        e_log_error("e_view_unfullscreen: workspace is NULL!");
        return;
    }

    //this view must be the one that's in fullscreen mode
    if (workspace->fullscreen_view != view)
        return;

    view->fullscreen = false;
    workspace->fullscreen_view = NULL;
    
    e_view_set_tiled(view, view->tiled);

    e_workspace_update_tree_visibility(workspace);
    e_workspace_arrange(workspace, workspace->full_area, workspace->tiled_area);
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

// Display view.
// Set fullscreen to true and set output if you want the view to be on a specific output immediately.
// If output is NULL, searches for current hovered output instead.
void e_view_map(struct e_view* view, bool fullscreen, struct e_output* output)
{
    assert(view);

    e_log_info("view map");

    struct e_server* server = view->server;

    if (output == NULL || output->active_workspace == NULL)
    {
        output = e_desktop_hovered_output(server);

        if (output == NULL || output->active_workspace == NULL)
        {
            e_log_error("e_view_map: unable to map view, no output with a workspace");
            return;
        }
    }

    wl_list_insert(&server->views, &view->link);

    view->content_tree = e_view_create_content_tree(view);

    if (view->content_tree == NULL)
    {
        e_log_error("e_view_map: unable to map view, failed to create content tree!");
        return;
    }

    wlr_scene_node_set_position(&view->tree->node, view->current.x, view->current.y);

    view->mapped = true;

    e_view_set_workspace(view, output->active_workspace);

    if (fullscreen)
        e_view_fullscreen(view);

    bool wants_floating = e_view_wants_floating(view);
    e_view_set_tiled(view, !wants_floating);

    wlr_scene_node_set_enabled(&view->tree->node, true);

    //set focus to this view
    if (view->surface != NULL)
        e_desktop_focus_view(view);
}

// Stop displaying view.
void e_view_unmap(struct e_view* view)
{   
    assert(view);

    #if E_VERBOSE
    e_log_info("view unmap");
    #endif

    e_view_move_to_workspace(view, NULL);

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

struct e_view* e_view_from_surface(struct e_server* server, struct wlr_surface* surface)
{
    assert(server);
    
    if (surface == NULL)
        return NULL;

    if (wl_list_empty(&server->views))
        return NULL;

    struct e_view* view;

    wl_list_for_each(view, &server->views, link)
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

// Raise view to the top of its parent tree.
void e_view_raise_to_top(struct e_view* view)
{
    if (view == NULL)
    {
        e_log_error("e_view_raise_to_top: view is NULL!");
        return;
    }

    if (view->tree == NULL)
    {
        e_log_error("e_view_raise_to_top: view has no tree!");
        return;
    }

    wlr_scene_node_raise_to_top(&view->tree->node);
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
