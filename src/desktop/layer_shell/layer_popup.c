#include "desktop/layer_shell.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_layer_shell_v1.h>

#include "desktop/tree/node.h"
#include "desktop/xdg_popup.h"

// New xdg popup.
static void e_layer_popup_new_popup(struct wl_listener* listener, void* data)
{
    struct e_xdg_popup* popup = wl_container_of(listener, popup, new_popup);
    struct wlr_xdg_popup* new_xdg_popup = data;

    e_xdg_popup_create(new_xdg_popup, popup->xdg_popup->base);
}

// New surface state got committed.
static void e_layer_popup_commit(struct wl_listener* listener, void* data)
{
    struct e_layer_popup* popup = wl_container_of(listener, popup, commit);
    
    if (popup->xdg_popup->base->initial_commit)
    {
        //TODO: ensure the popup window isn't positioned offscreen
        wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
    }
}

// Layer popup got destroyed.
static void e_layer_popup_destroy(struct wl_listener* listener, void* data)
{
    struct e_layer_popup* popup = wl_container_of(listener, popup, destroy);

    wl_list_remove(&popup->new_popup.link);
    wl_list_remove(&popup->commit.link);
    wl_list_remove(&popup->destroy.link);

    free(popup);
}

// Creates new layer popup for layer surface.
// Returns NULL on fail.
struct e_layer_popup* e_layer_popup_create(struct wlr_xdg_popup* xdg_popup, struct wlr_layer_surface_v1* layer_surface_v1)
{
    assert(xdg_popup && layer_surface_v1);

    struct e_layer_popup* popup = calloc(1, sizeof(struct e_layer_popup));
    popup->xdg_popup = xdg_popup;
    popup->layer_surface_v1 = layer_surface_v1;

    //create popup window's scene tree, and add popup to scene tree of parent
    struct wlr_scene_tree* parent_tree = layer_surface_v1->data;

    popup->scene_tree = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);
    e_node_desc_create(&popup->scene_tree->node, E_NODE_DESC_LAYER_POPUP, popup);

    //allows further popup window scene trees to add themselves to this popup window's scene tree
    xdg_popup->base->data = popup->scene_tree;

    //events
    popup->new_popup.notify = e_layer_popup_new_popup;
    wl_signal_add(&xdg_popup->base->events.new_popup, &popup->new_popup);

    popup->commit.notify = e_layer_popup_commit;
    wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

    popup->destroy.notify = e_layer_popup_destroy;
    wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);

    return popup;
}