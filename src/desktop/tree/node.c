#include "desktop/tree/node.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>

static void e_node_desc_destroy(struct wl_listener* listener, void* data)
{
    struct e_node_desc* node_desc = wl_container_of(listener, node_desc, destroy);
    
    free(node_desc);
}

//attaches to the data of the node, and is automatically destroyed on node destruction
struct e_node_desc* e_node_desc_create(struct wlr_scene_node* node, enum e_node_desc_type type, void* data)
{
    struct e_node_desc* node_desc = calloc(1, sizeof(*node_desc));
    node_desc->type = type;
    node_desc->data = data;

    node_desc->destroy.notify = e_node_desc_destroy;
    wl_signal_add(&node->events.destroy, &node_desc->destroy);

    node->data = node_desc;

    return node_desc;
}

//may return NULL
struct e_window* e_window_try_from_e_node_desc(struct e_node_desc *node_desc) 
{
    if (node_desc->type != E_NODE_DESC_WINDOW)
        return NULL;

    return (struct e_window *)node_desc->data;
}

//may return NULL
struct e_xdg_popup* e_xdg_popup_try_from_e_node_desc(struct e_node_desc* node_desc)
{
    if (node_desc->type != E_NODE_DESC_XDG_POPUP)
        return NULL;

    return (struct e_xdg_popup*)node_desc->data;
}

//may return NULL
struct e_layer_surface* e_layer_surface_try_from_e_node_desc(struct e_node_desc* node_desc)
{
    if (node_desc->type != E_NODE_DESC_LAYER_SURFACE)
        return NULL;

    return (struct e_layer_surface*)node_desc->data;
}

//may return NULL
struct e_layer_popup* e_layer_popup_try_from_e_node_desc(struct e_node_desc* node_desc)
{
    if (node_desc->type != E_NODE_DESC_LAYER_POPUP)
        return NULL;

    return (struct e_layer_popup*)node_desc->data;
}