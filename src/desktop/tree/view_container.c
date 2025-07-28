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

#include "desktop/tree/node.h"
#include "desktop/views/view.h"
#include "desktop/tree/workspace.h"
#include "desktop/desktop.h"
#include "desktop/output.h"

#include "util/log.h"
#include "util/wl_macros.h"

#include "server.h"

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

    //TODO: fullscreen

    e_log_info("map container wants fullscreen: %i", event->fullscreen);
    
    if (!event->wants_floating)
        e_workspace_add_tiled_container(workspace, &view_container->base);
    else
        e_workspace_add_floating_container(workspace, &view_container->base);


    e_workspace_rearrange(workspace);
    
    //TODO: tiled -> parent to previously focused tiled container
    //TODO: floating -> set container area from view geometry and center

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

static void e_view_container_handle_view_commit(struct wl_listener* listener, void* data)
{
    struct e_view_container* view_container = wl_container_of(listener, view_container, commit);

    //TODO: view tree node position needs to be updated immediately if only position is changed, but after a geometry update (not just a commit) if size was also changed

    view_container->view_current = view_container->view_pending;
    wlr_scene_node_set_position(&view_container->view->tree->node, view_container->view_current.x, view_container->view_current.y);

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

static void e_view_container_handle_view_request_configure(struct wl_listener* listener, void* data)
{
    struct e_view_container* view_container = wl_container_of(listener, view_container, request_configure);
    struct e_view_request_configure_event* event = data;

    //only respect configure request if container is floating, otherwise respond with our pending size

    if (!e_container_is_tiled(&view_container->base)) //floating
        e_container_arrange(&view_container->base, (struct wlr_box){event->x, event->y, event->width, event->height});
    else //tiled
        e_view_configure(view_container->view, view_container->view_pending.x, view_container->view_pending.y, view_container->view->geometry.width, view_container->view->geometry.height);
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

    SIGNAL_CONNECT(view->events.request_configure, view_container->request_configure, e_view_container_handle_view_request_configure);

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