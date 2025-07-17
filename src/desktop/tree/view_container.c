#include "desktop/tree/container.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include "desktop/views/view.h"

#include "util/log.h"
#include "util/wl_macros.h"

#include "server.h"

static void e_view_container_destroy(struct e_view_container* view_container)
{
    assert(view_container);

    if (view_container == NULL)
    {
        e_log_error("e_view_container_destroy: view_container is NULL!");
        return;
    }

    //reparent view node before destroying container node, so we don't destroy the view's tree aswell
    wlr_scene_node_reparent(&view_container->view->tree->node, view_container->view->server->pending);
    wlr_scene_node_destroy(&view_container->tree->node);

    SIGNAL_DISCONNECT(view_container->map);
    SIGNAL_DISCONNECT(view_container->unmap);
    
    SIGNAL_DISCONNECT(view_container->destroy);

    e_container_fini(&view_container->base);

    free(view_container);
}

static void e_view_container_impl_configure(struct e_container* container, int lx, int ly, int width, int height)
{
    //TODO: view container impl configure
}

static void e_view_container_impl_destroy(struct e_container* container)
{
    assert(container);

    if (container == NULL)
    {
        e_log_error("e_view_container_impl_destroy: container is NULL");
        return;
    }

    struct e_view_container* view_container = wl_container_of(container, view_container, base);

    e_view_container_destroy(view_container);
}

static const struct e_container_impl view_container_impl = {
    .configure = e_view_container_impl_configure,
    .destroy = e_view_container_impl_destroy
};

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

    e_view_container_destroy(view_container);
}

// Create a view container.
// Returns NULL on fail.
struct e_view_container* e_view_container_create(struct e_view* view)
{
    assert(view);

    if (view != NULL)
        return NULL;

    struct e_view_container* view_container = calloc(1, sizeof(*view_container));

    if (!e_container_init(&view_container->base, &view_container_impl, E_CONTAINER_VIEW, view_container))
    {
        free(view_container);
        return NULL;
    }

    view_container->tree = wlr_scene_tree_create(view->server->pending);

    if (view_container->tree == NULL)
    {
        free(view_container);
        return NULL;
    }

    view_container->view = view;

    wlr_scene_node_reparent(&view->tree->node, view_container->tree);

    SIGNAL_CONNECT(view->events.map, view_container->map, e_view_container_handle_view_map);
    SIGNAL_CONNECT(view->events.unmap, view_container->unmap, e_view_container_handle_view_unmap);

    SIGNAL_CONNECT(view->events.destroy, view_container->destroy, e_view_container_handle_view_destroy);

    return NULL;
}