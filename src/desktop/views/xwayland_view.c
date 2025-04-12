#include "desktop/views/xwayland_view.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>
#include <wlr/xwayland.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xproto.h>

#include "desktop/desktop.h"
#include "desktop/tree/node.h"
#include "desktop/views/view.h"

#include "input/cursor.h"

#include "util/log.h"

static void e_xwayland_view_map(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, map);

    e_view_map(&xwayland_view->base);
}

static void e_xwayland_view_unmap(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, unmap);

    e_view_unmap(&xwayland_view->base);
}

static void e_xwayland_view_commit(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, commit);

    xwayland_view->base.current.width = xwayland_view->xwayland_surface->width;
    xwayland_view->base.current.height = xwayland_view->xwayland_surface->height;

    //TODO: implement e_xwayland_view_commit
}

static void e_xwayland_view_request_maximize(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, request_configure);
    
    //only maximize floating views
    if (!xwayland_view->base.tiled)
        e_view_maximize(&xwayland_view->base);
}

static void e_xwayland_view_request_configure(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, request_configure);
    struct wlr_xwayland_surface_configure_event* event = data;
    
    if (!xwayland_view->base.tiled)
    {
        e_view_set_position(&xwayland_view->base, event->x, event->y);
        e_view_set_size(&xwayland_view->base, event->width, event->height);
    }
}

static void e_xwayland_view_request_move(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, request_move);
    
    struct e_desktop* desktop = xwayland_view->base.desktop;
    e_cursor_start_window_move(desktop->seat->cursor, xwayland_view->base.container);
}

static void e_xwayland_view_request_resize(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, request_resize);
    struct wlr_xwayland_resize_event* event = data;
    
    struct e_desktop* desktop = xwayland_view->base.desktop;
    e_cursor_start_window_resize(desktop->seat->cursor, xwayland_view->base.container, event->edges);
}

static void e_xwayland_view_set_title(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, set_title);

    xwayland_view->base.title = xwayland_view->xwayland_surface->title;
    wl_signal_emit(&xwayland_view->base.events.set_title, xwayland_view->xwayland_surface->title);
}

//surface becomes valid, like me!
static void e_xwayland_view_associate(struct wl_listener* listener, void* data)
{
    //surface is valid
    e_log_info("associate: xwayland view surface is now valid");

    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, associate);

    xwayland_view->base.surface = xwayland_view->xwayland_surface->surface;

    // events

    xwayland_view->map.notify = e_xwayland_view_map;
    wl_signal_add(&xwayland_view->xwayland_surface->surface->events.map, &xwayland_view->map);

    xwayland_view->unmap.notify = e_xwayland_view_unmap;
    wl_signal_add(&xwayland_view->xwayland_surface->surface->events.unmap, &xwayland_view->unmap);

    xwayland_view->commit.notify = e_xwayland_view_commit;
    wl_signal_add(&xwayland_view->xwayland_surface->surface->events.commit, &xwayland_view->commit);

    // requests

    xwayland_view->request_maximize.notify = e_xwayland_view_request_maximize;
    wl_signal_add(&xwayland_view->xwayland_surface->events.request_maximize, &xwayland_view->request_maximize);

    xwayland_view->request_configure.notify = e_xwayland_view_request_configure;
    wl_signal_add(&xwayland_view->xwayland_surface->events.request_configure, &xwayland_view->request_configure);

    xwayland_view->request_move.notify = e_xwayland_view_request_move;
    wl_signal_add(&xwayland_view->xwayland_surface->events.request_move, &xwayland_view->request_move);
    
    xwayland_view->request_resize.notify = e_xwayland_view_request_resize;
    wl_signal_add(&xwayland_view->xwayland_surface->events.request_resize, &xwayland_view->request_resize);

    // other events

    xwayland_view->set_title.notify = e_xwayland_view_set_title;
    wl_signal_add(&xwayland_view->xwayland_surface->events.set_title, &xwayland_view->set_title);

    //add surface & subsurfaces to scene by creating a subsurface tree
    struct e_desktop* desktop = xwayland_view->base.desktop;
    xwayland_view->base.tree = wlr_scene_subsurface_tree_create(desktop->pending, xwayland_view->xwayland_surface->surface);
    e_node_desc_create(&xwayland_view->base.tree->node, E_NODE_DESC_VIEW, &xwayland_view->base);
}

//surface becomes invalid
static void e_xwayland_view_dissociate(struct wl_listener* listener, void* data)
{
    //surface is now invalid
    e_log_info("dissociate: xwayland view surface is now invalid");

    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, dissociate);

    xwayland_view->base.surface = NULL;

    wl_list_remove(&xwayland_view->map.link);
    wl_list_remove(&xwayland_view->unmap.link);
    wl_list_remove(&xwayland_view->commit.link);

    wl_list_remove(&xwayland_view->request_maximize.link);
    wl_list_remove(&xwayland_view->request_configure.link);
    wl_list_remove(&xwayland_view->request_move.link);
    wl_list_remove(&xwayland_view->request_resize.link);

    wl_list_remove(&xwayland_view->set_title.link);

    xwayland_view->base.tree = NULL;
    wlr_scene_node_destroy(&xwayland_view->base.tree->node);
}

//destruction...
static void e_xwayland_view_destroy(struct wl_listener* listener, void* data)
{
    //remove associate & dissociate listener lists & free

    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, destroy);

    wl_list_remove(&xwayland_view->associate.link);
    wl_list_remove(&xwayland_view->dissociate.link);
    wl_list_remove(&xwayland_view->destroy.link);

    e_view_fini(&xwayland_view->base);

    free(xwayland_view);
}

static uint32_t e_view_xwayland_configure(struct e_view* view, int lx, int ly, int width, int height)
{
    assert(view && view->tree);

    wlr_scene_node_set_position(&view->tree->node, lx, ly);

    struct e_xwayland_view* xwayland_view = view->data;

    wlr_xwayland_surface_configure(xwayland_view->xwayland_surface, lx, ly, (uint16_t)width, (uint16_t)height);

    return 0;
}

static bool e_view_xwayland_wants_floating(struct e_view* view)
{
    assert(view);

    struct e_xwayland_view* xwayland_view = view->data;
    struct wlr_xwayland_surface* xwayland_surface = xwayland_view->xwayland_surface;

    //does surface want to be above every window?
    if (xwayland_surface->modal)
        return true;

    //does surface force a specific size?
    xcb_size_hints_t* size_hints = xwayland_surface->size_hints;

    if (size_hints != NULL && (size_hints->min_width > 0 || size_hints->min_height > 0) && (size_hints->min_width == size_hints->max_width|| size_hints->min_width == size_hints->max_width))
        return true;

    //TODO: does surface contain a window type xcb atom that should always float?
    /*
    for (size_t i = 0; i < xwayland_surface->window_type_len; i++)
    {
        xcb_atom_t window_type = xwayland_surface->window_type[i];
        
        ...
    }
    */

    //FIXME: not all override redirect window should float, but this is the best we got right now as we can't check for atoms.
    if (xwayland_surface->override_redirect)
        return true;

    return false;
}

static void e_view_xwayland_send_close(struct e_view* view)
{
    assert(view && view->data);

    struct e_xwayland_view* xwayland_view = view->data;

    wlr_xwayland_surface_close(xwayland_view->xwayland_surface);
}

//creates new xwayland view inside server
struct e_xwayland_view* e_xwayland_view_create(struct e_desktop* desktop, struct wlr_xwayland_surface* xwayland_surface)
{
    assert(desktop && xwayland_surface);
    
    e_log_info("new xwayland view");

    struct e_xwayland_view* xwayland_view = calloc(1, sizeof(*xwayland_view));

    if (xwayland_view == NULL)
    {
        e_log_error("failed to allocate xwayland_view");
        return NULL;
    }

    xwayland_view->xwayland_surface = xwayland_surface;

    e_view_init(&xwayland_view->base, desktop, E_VIEW_XWAYLAND, xwayland_view);

    xwayland_view->base.title = xwayland_surface->title;

    xwayland_view->base.implementation.configure = e_view_xwayland_configure;
    xwayland_view->base.implementation.wants_floating = e_view_xwayland_wants_floating;
    xwayland_view->base.implementation.send_close = e_view_xwayland_send_close;

    // events

    xwayland_view->associate.notify = e_xwayland_view_associate;
    wl_signal_add(&xwayland_surface->events.associate, &xwayland_view->associate);
    
    xwayland_view->dissociate.notify = e_xwayland_view_dissociate;
    wl_signal_add(&xwayland_surface->events.dissociate, &xwayland_view->dissociate);

    xwayland_view->destroy.notify = e_xwayland_view_destroy;
    wl_signal_add(&xwayland_surface->events.destroy, &xwayland_view->destroy);

    return xwayland_view;
}