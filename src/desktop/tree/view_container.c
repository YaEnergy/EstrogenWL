#include "desktop/tree/container.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

#include <wlr/util/box.h>

#include "desktop/tree/node.h"
#include "desktop/views/view.h"
#include "desktop/tree/workspace.h"

#include "util/log.h"
#include "util/wl_macros.h"

#include "server.h"

static void e_view_container_handle_view_map(struct wl_listener* listener, void* data)
{
    //TODO: e_view_container_handle_view_map
}

static void e_view_container_handle_view_unmap(struct wl_listener* listener, void* data)
{
    //TODO: e_view_container_handle_view_unmap
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