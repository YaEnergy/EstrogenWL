#include "desktop/windows/popup.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>

#include "util/log.h"

//new surface state got committed
static void e_popup_commit(struct wl_listener* listener, void* data)
{
    struct e_popup* popup = wl_container_of(listener, popup, commit);
    
    if (popup->xdg_popup->base->initial_commit)
    {
        //TODO: ensure the popup window isn't positioned offscreen
        wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
    }
}

//xdg_popup got destroyed
static void e_popup_destroy(struct wl_listener* listener, void* data)
{
    struct e_popup* popup = wl_container_of(listener, popup, destroy);

    wl_list_remove(&popup->commit.link);
    wl_list_remove(&popup->destroy.link);

    free(popup);
}

struct e_popup* e_popup_create(struct wlr_xdg_popup* xdg_popup)
{
    struct e_popup* popup = calloc(1, sizeof(struct e_popup));

    popup->xdg_popup = xdg_popup;

    //create popup window's scene tree, and add popup to scene tree of parent
    struct wlr_xdg_surface* parent_surface = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);

    if (parent_surface == NULL)
    {
        e_log_error("failed to get parent surface of xdg_popup");
        abort();
    }

    struct wlr_scene_tree* parent_tree = parent_surface->data;

    popup->scene_tree = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);
    popup->scene_tree->node.data = popup;

    //allows further popup window scene trees to add themselves to this popup window's scene tree
    xdg_popup->base->data = popup->scene_tree;

    //events
    popup->commit.notify = e_popup_commit;
    wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

    popup->destroy.notify = e_popup_destroy;
    wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);

    return popup;
}