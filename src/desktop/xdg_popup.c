#include "desktop/xdg_popup.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>

#include "desktop/tree/node.h"
#include "util/log.h"

//another xdg_popup? oh woaw
static void e_xdg_popup_new_popup(struct wl_listener* listener, void* data)
{
    struct e_xdg_popup* popup = wl_container_of(listener, popup, new_popup);
    struct wlr_xdg_popup* new_xdg_popup = data;

    e_xdg_popup_create(new_xdg_popup, popup->xdg_popup->base);
}

//new surface state got committed
static void e_xdg_popup_commit(struct wl_listener* listener, void* data)
{
    struct e_xdg_popup* popup = wl_container_of(listener, popup, commit);
    
    if (popup->xdg_popup->base->initial_commit)
    {
        //TODO: ensure the popup window isn't positioned offscreen
        wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
    }
}

//xdg_popup got destroyed
static void e_xdg_popup_destroy(struct wl_listener* listener, void* data)
{
    struct e_xdg_popup* popup = wl_container_of(listener, popup, destroy);

    wl_list_remove(&popup->new_popup.link);
    wl_list_remove(&popup->commit.link);
    wl_list_remove(&popup->destroy.link);

    free(popup);
}

struct e_xdg_popup* e_xdg_popup_create(struct wlr_xdg_popup* xdg_popup, struct wlr_xdg_surface* xdg_surface)
{
    assert(xdg_popup && xdg_surface);

    struct e_xdg_popup* popup = calloc(1, sizeof(*popup));

    if (popup == NULL)
    {
        e_log_error("failed to allocate xdg_popup");
        return NULL;
    }

    popup->xdg_popup = xdg_popup;
    popup->xdg_surface = xdg_surface;

    //create popup window's scene tree, and add popup to scene tree of parent
    struct wlr_scene_tree* parent_tree = xdg_surface->data;

    popup->scene_tree = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);
    e_node_desc_create(&popup->scene_tree->node, E_NODE_DESC_XDG_POPUP, popup);

    //allows further popup window scene trees to add themselves to this popup window's scene tree
    xdg_popup->base->data = popup->scene_tree;

    //events
    popup->new_popup.notify = e_xdg_popup_new_popup;
    wl_signal_add(&xdg_popup->base->events.new_popup, &popup->new_popup);

    popup->commit.notify = e_xdg_popup_commit;
    wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

    popup->destroy.notify = e_xdg_popup_destroy;
    wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);

    return popup;
}