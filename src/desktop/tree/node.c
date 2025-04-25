#include "desktop/tree/node.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>

#include "util/log.h"
#include "util/wl_macros.h"

static void e_node_desc_destroy(struct wl_listener* listener, void* data)
{
    struct e_node_desc* node_desc = wl_container_of(listener, node_desc, destroy);

    SIGNAL_DISCONNECT(node_desc->destroy);
    
    free(node_desc);
}

//attaches to the data of the node, and is automatically destroyed on node destruction
struct e_node_desc* e_node_desc_create(struct wlr_scene_node* node, enum e_node_desc_type type, void* data)
{
    struct e_node_desc* node_desc = calloc(1, sizeof(*node_desc));

    if (node_desc == NULL)
    {
        e_log_error("e_node_desc_create: failed to alloc e_node_desc");
        return NULL;
    }

    node_desc->type = type;
    node_desc->data = data;

    SIGNAL_CONNECT(node->events.destroy, node_desc->destroy, e_node_desc_destroy);

    node->data = node_desc;

    return node_desc;
}

//may return NULL
struct e_view* e_view_try_from_e_node_desc(struct e_node_desc* node_desc) 
{
    assert(node_desc);

    if (node_desc->type != E_NODE_DESC_VIEW)
        return NULL;

    return (struct e_view*)node_desc->data;
}

//may return NULL
struct e_xdg_popup* e_xdg_popup_try_from_e_node_desc(struct e_node_desc* node_desc)
{
    assert(node_desc);

    if (node_desc->type != E_NODE_DESC_XDG_POPUP)
        return NULL;

    return (struct e_xdg_popup*)node_desc->data;
}

//may return NULL
struct e_layer_surface* e_layer_surface_try_from_e_node_desc(struct e_node_desc* node_desc)
{
    assert(node_desc);

    if (node_desc->type != E_NODE_DESC_LAYER_SURFACE)
        return NULL;

    return (struct e_layer_surface*)node_desc->data;
}
