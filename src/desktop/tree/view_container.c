#include "desktop/tree/container.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>

#include <wlr/util/box.h>
#include <wlr/util/edges.h>

#include "desktop/tree/node.h"
#include "desktop/views/view.h"
#include "desktop/tree/workspace.h"
#include "desktop/desktop.h"
#include "desktop/output.h"

#include "input/seat.h"
#include "input/cursor.h"

#include "util/log.h"
#include "util/wl_macros.h"

#include "server.h"

static void view_container_set_content_position(struct e_view_container* view_container, int x, int y)
{
    assert(view_container);

    wlr_scene_node_set_position(&view_container->view->tree->node, x, y);
}

// Container must be arranged after. (Rearranged in this case)
static void view_container_set_size_from_view(struct e_view_container* view_container)
{
    assert(view_container);

    view_container->view_pending.width = view_container->view->width;
    view_container->view_pending.height = view_container->view->height;

    view_container->view_current.width = view_container->view->width;
    view_container->view_current.height = view_container->view->height;

    view_container->base.area.width = view_container->view->width;
    view_container->base.area.height = view_container->view->height;
}

static void e_view_container_handle_view_map(struct wl_listener* listener, void* data)
{
    struct e_view_container* view_container = wl_container_of(listener, view_container, map);
    struct e_view_map_event* event = data;

    struct e_workspace* workspace = NULL;

    if (event->fullscreen && event->fullscreen_output != NULL)
    {
        workspace = event->fullscreen_output->active_workspace;
    }
    else 
    {
        struct e_output* output = e_desktop_hovered_output(view_container->base.server);

        if (output != NULL)
            workspace = output->active_workspace;
    }

    if (workspace == NULL)
    {
        e_log_error("e_view_container_handle_view_map: couldn't find workspace for view");
        return;
    }

    //TODO: xwayland views may want to specify their position through their size hints

    e_log_info("map container wants fullscreen: %i", event->fullscreen);
    
    e_container_set_fullscreen(&view_container->base, event->fullscreen);
    
    if (!event->wants_floating)
    {
        e_workspace_add_tiled_container(workspace, &view_container->base);
    }
    else
    {
        e_workspace_add_floating_container(workspace, &view_container->base);
        
        // If container hasn't been arranged yet (for ex. from a configure request), then do so 
        bool arranged = (!wlr_box_empty(&view_container->base.area)
            && !wlr_box_empty(&view_container->view_current) 
            && !wlr_box_empty(&view_container->view_pending));

        if (!arranged)
        {
            view_container_set_size_from_view(view_container);
            e_container_center_in_output(&view_container->base);
        }
    }

    e_workspace_rearrange(workspace);
    
    //set to pending position immediately, so we're not at (0,0)
    view_container_set_content_position(view_container, view_container->view_pending.x, view_container->view_pending.y);
    
    //TODO: tiled -> parent to previously focused tiled container

    e_desktop_focus_view_container(view_container);
}

static void e_view_container_handle_view_unmap(struct wl_listener* listener, void* data)
{
    struct e_view_container* view_container = wl_container_of(listener, view_container, unmap);

    struct e_workspace* workspace = view_container->base.workspace;

    e_container_leave(&view_container->base);

    if (workspace != NULL)
        e_workspace_rearrange(workspace);
}

static void view_container_update_popup_space(struct e_view_container* view_container)
{
    assert(view_container);

    struct e_output* output = view_container->view->output;

    if (output == NULL)
        return;

    struct wlr_box layout_output_box = (struct wlr_box){0, 0, 0, 0};
    wlr_output_layout_get_box(output->layout, output->wlr_output, &layout_output_box);

    struct wlr_box toplevel_popup_space = {
        .x = layout_output_box.x - view_container->view_current.x,
        .y = layout_output_box.y - view_container->view_current.y,
        .width = layout_output_box.width,
        .height = layout_output_box.height
    };

    e_view_set_popup_space(view_container->view, toplevel_popup_space);
}

// Applies the new geometry to the view container and its view.
// As view may commit sizes that are different from what we requested, they may not match view_pending.
static void view_container_apply_geometry(struct e_view_container* view_container, int width, int height)
{
    assert(view_container);

    //when resizing interactively, edges opposite the grabbed edges need to be anchored
    //when grabbing right or bottom edge, left and top are automatically anchored
    //but when grabbing left or top edge, right and bottom need to be anchored

    //because views can commit sizes that are different from what we requested, (they may not match view_pending) 
    //we need to take this into account when anchoring the edges

    //I struggle to wrap my head around this problem, but the solution is easier to wrap my head around
    //I would prefer not having to touch cursor from the seat & server too much in here though...

    //TODO: center view if container is tiled?

    view_container->view_current.width = width;
    view_container->view_current.height = height;

    struct e_cursor* cursor = view_container->base.server->seat->cursor;
    uint32_t grabbed_edges = cursor->grab_edges;
    bool grabbed = (cursor->grab_container == &view_container->base);

    if (grabbed && (grabbed_edges & WLR_EDGE_LEFT))
        view_container->view_current.x = view_container->view_pending.x + view_container->view_pending.width - width;
    else
        view_container->view_current.x = view_container->view_pending.x;

    if (grabbed && (grabbed_edges & WLR_EDGE_TOP))
        view_container->view_current.y = view_container->view_pending.y + view_container->view_pending.height - height;
    else
        view_container->view_current.y = view_container->view_pending.y;

    //update container area if container is floating, as they should be the same size in this case
    if (!e_container_is_tiled(&view_container->base))
        view_container->base.area = view_container->view_current;

    view_container_set_content_position(view_container, view_container->view_current.x, view_container->view_current.y);
    view_container_update_popup_space(view_container);

    //keep in sync when no requests are pending
    view_container->view_pending = view_container->view_current;
}

static void e_view_container_handle_view_commit(struct wl_listener* listener, void* data)
{
    struct e_view_container* view_container = wl_container_of(listener, view_container, commit);

    //TODO: view tree node position needs to be updated immediately if only position is changed (not after a commit, like currently), but after a geometry update (not just a commit) if size was also changed

    //TODO: center view if container is tiled?

    bool size_changed = (view_container->view_current.width != view_container->view->width
        || view_container->view_current.height != view_container->view->height);

    bool size_pending = (view_container->view_current.width != view_container->view_pending.width
        || view_container->view_current.height != view_container->view_pending.height);

    bool position_pending = (view_container->view_current.x != view_container->view_pending.x
        || view_container->view_current.y != view_container->view_pending.y);

    //size changed or position pending but no size pending
    if (size_changed || (!size_pending && position_pending))
        view_container_apply_geometry(view_container, view_container->view->width, view_container->view->height);
}

static void e_view_container_handle_view_request_move(struct wl_listener* listener, void* data)
{
    struct e_view_container* view_container = wl_container_of(listener, view_container, request_move);

    e_cursor_start_container_move(view_container->base.server->seat->cursor, &view_container->base);
}

static void e_view_container_handle_view_request_resize(struct wl_listener* listener, void* data)
{
    struct e_view_container* view_container = wl_container_of(listener, view_container, request_resize);
    struct e_view_request_resize_event* event = data;

    e_cursor_start_container_resize(view_container->base.server->seat->cursor, &view_container->base, event->edges);
}

static void e_view_container_handle_view_request_configure(struct wl_listener* listener, void* data)
{
    struct e_view_container* view_container = wl_container_of(listener, view_container, request_configure);
    struct e_view_request_configure_event* event = data;

    //only respect configure request if container is floating, otherwise respond with our pending size

    if (!e_container_is_tiled(&view_container->base)) //floating
    {
        view_container->base.area = (struct wlr_box){event->x, event->y, event->width, event->height};
        e_container_arrange(&view_container->base);
    }
    else //tiled
    {
        e_view_configure(view_container->view, view_container->view_pending.x, view_container->view_pending.y, view_container->view_pending.width, view_container->view_pending.height);
    }
}

static void e_view_container_handle_view_request_fullscreen(struct wl_listener* listener, void* data)
{
    struct e_view_container* view_container = wl_container_of(listener, view_container, request_fullscreen);
    struct e_view_request_fullscreen_event* event = data;

    //TODO: respect output if not mapped?
    //TODO: init initial output geometry?
    if (!view_container->view->mapped)
    {
        e_container_set_fullscreen(&view_container->base, event->fullscreen);
        return;
    }

    struct e_workspace* old_workspace = view_container->base.workspace;
    struct e_workspace* new_workspace = view_container->base.workspace;
    
    if (event->fullscreen && event->output != NULL && event->output->active_workspace != NULL)
        new_workspace = event->output->active_workspace;

    //just set fullscreen mode
    if (old_workspace == NULL && new_workspace == NULL)
    {
        e_log_error("e_view_container_handle_view_request_fullscreen: old & new workspace are somehow NULL.");
        e_container_set_fullscreen(&view_container->base, event->fullscreen);
        return;
    }

    if (old_workspace != new_workspace)
        e_container_move_to_workspace(&view_container->base, new_workspace);
    
    if (event->fullscreen && !view_container->base.fullscreen)
        e_workspace_change_fullscreen_container(new_workspace, &view_container->base);
    else if (!event->fullscreen && view_container->base.fullscreen)
        e_workspace_change_fullscreen_container(new_workspace, NULL);

    e_workspace_rearrange(old_workspace);
    
    if (old_workspace != new_workspace)
        e_workspace_rearrange(new_workspace);
}

static void e_view_container_handle_view_destroy(struct wl_listener* listener, void* data)
{
    struct e_view_container* view_container = wl_container_of(listener, view_container, destroy);

    e_container_destroy(&view_container->base);
}

// Create a view container.
// Returns NULL on fail.
struct e_view_container* e_view_container_create(struct e_server* server, struct e_view* view)
{
    assert(server && view);

    if (server == NULL || view == NULL)
        return NULL;

    struct e_view_container* view_container = calloc(1, sizeof(*view_container));

    if (!e_container_init(&view_container->base, E_CONTAINER_VIEW, server))
    {
        free(view_container);
        return NULL;
    }

    view_container->base.view_container = view_container;

    view_container->view = view;

    wlr_scene_node_reparent(&view->tree->node, view_container->base.tree);

    SIGNAL_CONNECT(view->events.map, view_container->map, e_view_container_handle_view_map);
    SIGNAL_CONNECT(view->events.unmap, view_container->unmap, e_view_container_handle_view_unmap);

    SIGNAL_CONNECT(view->events.commit, view_container->commit, e_view_container_handle_view_commit);

    SIGNAL_CONNECT(view->events.request_move, view_container->request_move, e_view_container_handle_view_request_move);
    SIGNAL_CONNECT(view->events.request_resize, view_container->request_resize, e_view_container_handle_view_request_resize);
    SIGNAL_CONNECT(view->events.request_configure, view_container->request_configure, e_view_container_handle_view_request_configure);
    SIGNAL_CONNECT(view->events.request_fullscreen, view_container->request_fullscreen, e_view_container_handle_view_request_fullscreen);

    SIGNAL_CONNECT(view->events.destroy, view_container->destroy, e_view_container_handle_view_destroy);

    wl_list_insert(&server->view_containers, &view_container->link);

    return view_container;
}

// Finds the view container which has this surface as its view's main surface.
// Returns NULL on fail.
struct e_view_container* e_view_container_try_from_surface(struct e_server* server, struct wlr_surface* surface)
{
    assert(server && surface);

    if (server == NULL || surface == NULL)
        return NULL;

    struct e_view_container* view_container;
    wl_list_for_each(view_container, &server->view_containers, link)
    {
        if (view_container->view->surface == surface)
            return view_container;
    }

    return NULL;
}