#include "desktop/tree/container.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

#include "desktop/tree/node.h"
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

    wl_list_remove(&view_container->link);

    //reparent view node before destroying container node, so we don't destroy the view's tree aswell
    wlr_scene_node_reparent(&view_container->view->tree->node, view_container->server->pending);
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
struct e_view_container* e_view_container_create(struct e_server* server, struct e_view* view)
{
    assert(server && view);

    if (server == NULL || view == NULL)
        return NULL;

    struct e_view_container* view_container = calloc(1, sizeof(*view_container));

    if (!e_container_init(&view_container->base, &view_container_impl, E_CONTAINER_VIEW, view_container))
    {
        free(view_container);
        return NULL;
    }

    view_container->tree = wlr_scene_tree_create(server->pending);

    if (view_container->tree == NULL)
    {
        free(view_container);
        return NULL;
    }

    if (e_node_desc_create(&view_container->tree->node, E_NODE_DESC_VIEW_CONTAINER, view_container) == NULL)
    {
        wlr_scene_node_destroy(&view_container->tree->node);
        free(view_container);
        return NULL;
    }

    view_container->server = server;
    view_container->view = view;

    wlr_scene_node_reparent(&view->tree->node, view_container->tree);

    SIGNAL_CONNECT(view->events.map, view_container->map, e_view_container_handle_view_map);
    SIGNAL_CONNECT(view->events.unmap, view_container->unmap, e_view_container_handle_view_unmap);

    SIGNAL_CONNECT(view->events.destroy, view_container->destroy, e_view_container_handle_view_destroy);

    wl_list_insert(&server->view_containers, &view_container->link);

    return NULL;
}

// Raise view container to the top of its layer. (floating, tiled & fullscreen but only matters for floating)
void e_view_container_raise_to_top(struct e_view_container* view_container)
{
    assert(view_container);

    if (view_container == NULL)
        return;

    wlr_scene_node_raise_to_top(&view_container->tree->node);
}

// Returns NULL on fail.
static struct e_view_container* e_view_container_try_from_node(struct wlr_scene_node* node)
{
    if (node == NULL)
        return NULL;

    //data is either NULL or e_node_desc
    if (node->data == NULL)
        return NULL;

    struct e_node_desc* node_desc = node->data;

    return e_view_container_try_from_e_node_desc(node_desc);
}

// Returns NULL on fail.
struct e_view_container* e_view_container_try_from_node_ancestors(struct wlr_scene_node* node)
{
    if (node == NULL)
        return NULL;

    struct e_view_container* view_container = e_view_container_try_from_node(node);

    if (view_container != NULL)
        return view_container;

    //keep going upwards in the tree until we find a view container (in which case we return it), or reach the root of the tree (no parent)
    while (node->parent != NULL)
    {
        //go to parent node
        node = &node->parent->node;

        view_container = e_view_container_try_from_node(node);

        if (view_container != NULL)
            return view_container;
    }

    return NULL;
}

// Finds the view container at the specified layout coords in given scene graph.
// Returns NULL on fail.
struct e_view_container* e_view_container_at(struct wlr_scene_node* node, double lx, double ly)
{
    if (node == NULL)
        return NULL;

    struct wlr_scene_node* node_at = wlr_scene_node_at(node, lx, ly, NULL, NULL);
    
    if (node_at != NULL)
        return e_view_container_try_from_node_ancestors(node_at);
    else
        return NULL;
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