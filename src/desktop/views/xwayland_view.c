#include "desktop/xwayland.h"

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
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
#include "util/wl_macros.h"

#include "server.h"

// Returns size hints of view.
static struct e_view_size_hints e_view_xwayland_get_size_hints(struct e_view* view)
{
    assert(view);

    struct e_xwayland_view* xwayland_view = view->data;
    struct wlr_xwayland_surface* surface = xwayland_view->xwayland_surface;

    if (surface->size_hints != NULL)
    {
        return (struct e_view_size_hints){
            .min_width = surface->size_hints->min_width,
            .min_height = surface->size_hints->min_height,
    
            .max_width = surface->size_hints->max_width,
            .max_height = surface->size_hints->max_height,

            .width_inc = surface->size_hints->width_inc,
            .height_inc = surface->size_hints->height_inc
        };
    }
    else 
    {
        return (struct e_view_size_hints){0};
    }
}

// Create a scene tree displaying this view's surfaces and subsurfaces.
static struct wlr_scene_tree* e_view_xwayland_create_content_tree(struct e_view* view)
{
    assert(view);

    //add surface & subsurfaces to scene by creating a subsurface tree
    struct wlr_scene_tree* tree = wlr_scene_subsurface_tree_create(view->tree, view->surface);
    e_node_desc_create(&tree->node, E_NODE_DESC_VIEW, view);

    return tree;
}

// Sets the view's current geometry to that of the xwayland surface.
static void e_xwayland_view_update_current_geometry(struct e_xwayland_view* xwayland_view)
{
    assert(xwayland_view);

    xwayland_view->base.current.x = xwayland_view->xwayland_surface->x;
    xwayland_view->base.current.y = xwayland_view->xwayland_surface->y;
    xwayland_view->base.current.width = xwayland_view->xwayland_surface->width;
    xwayland_view->base.current.height = xwayland_view->xwayland_surface->height;
}

// Xwayland surface wants to be mapped.
static void e_xwayland_view_map_request(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, map_request);

    #if E_VERBOSE
    e_log_info("xwayland view map request");
    #endif

    e_xwayland_view_update_current_geometry(xwayland_view);
    xwayland_view->base.pending = xwayland_view->base.current;

    e_view_configure_pending(&xwayland_view->base);
}

// New surface state got committed.
static void e_xwayland_view_commit(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, commit);

    e_xwayland_view_update_current_geometry(xwayland_view);

    if (e_view_has_pending_changes(&xwayland_view->base))
        e_view_configure_pending(&xwayland_view->base);
}

// Surface is ready to be displayed.
static void e_xwayland_view_map(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, map);

    e_view_map(&xwayland_view->base, xwayland_view->xwayland_surface->fullscreen, NULL);

    // According to labwc, map and unmap can change the surface used
    SIGNAL_CONNECT(xwayland_view->xwayland_surface->surface->events.commit, xwayland_view->commit, e_xwayland_view_commit);
}

// Surface no longer wants to be displayed.
static void e_xwayland_view_unmap(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, unmap);

    e_view_unmap(&xwayland_view->base);

    /* According to labwc, map and unmap can change the surface used */
    SIGNAL_DISCONNECT(xwayland_view->commit);
}

// Sets the tiled state of the view.
static void e_view_xwayland_set_tiled(struct e_view* view, bool tiled)
{
    assert(view);

    struct e_xwayland_view* xwayland_view = view->data;

    //TODO: if view is actually maximized, don't let e_view_xwayland_set_tiled set maximized to false
    
    //why set maximized? for example: this prevents Krita from requesting resizing from the bottom right corner
    wlr_xwayland_surface_set_maximized(xwayland_view->xwayland_surface, tiled, tiled);
}

// Set activated state of the view.
static void e_view_xwayland_set_activated(struct e_view* view, bool activated)
{
    assert(view);

    struct e_xwayland_view* xwayland_view = view->data;

    wlr_xwayland_surface_activate(xwayland_view->xwayland_surface, activated);
}

static void e_view_xwayland_set_fullscreen(struct e_view* view, bool fullscreen)
{
    assert(view && view->data);

    struct e_xwayland_view* xwayland_view = view->data;

    wlr_xwayland_surface_set_fullscreen(xwayland_view->xwayland_surface, fullscreen);
}

static void e_xwayland_view_request_fullscreen(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, request_fullscreen);

    #if E_VERBOSE
    e_log_info("xwayland view request fullscreen");
    #endif

    e_view_set_fullscreen(&xwayland_view->base, !xwayland_view->base.fullscreen);
}

static void e_xwayland_view_request_maximize(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, request_configure);

    #if E_VERBOSE
    e_log_info("xwayland view request maximize");
    #endif

    //TODO: xwayland view request maximize
    //only maximize floating views
}

// Xwayland surface wants to be configured in a specific way.
static void e_xwayland_view_request_configure(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, request_configure);
    struct wlr_xwayland_surface_configure_event* event = data;
    
    #if E_VERBOSE
    e_log_info("xwayland view request configure");
    #endif

    //respect configure request if view is floating, otherwise don't
    if (!xwayland_view->base.tiled)
        e_view_configure(&xwayland_view->base, event->x, event->y, event->width, event->height);
    else
        e_view_configure_pending(&xwayland_view->base);
}

static void e_xwayland_view_request_move(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, request_move);
    
    struct e_server* server = xwayland_view->base.server;
    e_cursor_start_view_move(server->seat->cursor, &xwayland_view->base);
}

static void e_xwayland_view_request_resize(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, request_resize);
    struct wlr_xwayland_resize_event* event = data;
    
    struct e_server* server = xwayland_view->base.server;
    e_cursor_start_view_resize(server->seat->cursor, &xwayland_view->base, event->edges);
}

static void e_xwayland_view_set_title(struct wl_listener* listener, void* data)
{
    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, set_title);

    xwayland_view->base.title = xwayland_view->xwayland_surface->title;
}

// Surface becomes valid, like me!
static void e_xwayland_view_associate(struct wl_listener* listener, void* data)
{
    //surface is valid
    e_log_info("associate: xwayland view surface is now valid");

    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, associate);

    xwayland_view->base.surface = xwayland_view->xwayland_surface->surface;

    e_xwayland_view_update_current_geometry(xwayland_view);

    // events

    SIGNAL_CONNECT(xwayland_view->xwayland_surface->surface->events.map, xwayland_view->map, e_xwayland_view_map);
    SIGNAL_CONNECT(xwayland_view->xwayland_surface->surface->events.unmap, xwayland_view->unmap, e_xwayland_view_unmap);
}

// Surface becomes invalid.
static void e_xwayland_view_dissociate(struct wl_listener* listener, void* data)
{
    e_log_info("dissociate: xwayland view surface is now invalid");

    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, dissociate);

    xwayland_view->base.surface = NULL;

    SIGNAL_DISCONNECT(xwayland_view->map);
    SIGNAL_DISCONNECT(xwayland_view->unmap);
}

// Xwayland surface was destroyed.
static void e_xwayland_view_destroy(struct wl_listener* listener, void* data)
{
    //remove associate & dissociate listener lists & free

    struct e_xwayland_view* xwayland_view = wl_container_of(listener, xwayland_view, destroy);

    SIGNAL_DISCONNECT(xwayland_view->set_title);
    SIGNAL_DISCONNECT(xwayland_view->map_request);

    SIGNAL_DISCONNECT(xwayland_view->request_fullscreen);
    SIGNAL_DISCONNECT(xwayland_view->request_maximize);
    SIGNAL_DISCONNECT(xwayland_view->request_configure);
    SIGNAL_DISCONNECT(xwayland_view->request_move);
    SIGNAL_DISCONNECT(xwayland_view->request_resize);

    SIGNAL_DISCONNECT(xwayland_view->associate);
    SIGNAL_DISCONNECT(xwayland_view->dissociate);

    SIGNAL_DISCONNECT(xwayland_view->destroy);

    e_view_fini(&xwayland_view->base);

    free(xwayland_view);
}

// FIXME: i'm unsure of this actually works, should probably check that
// it does not
static bool xwayland_surface_geo_configure_is_scheduled(struct wlr_xwayland_surface* xwayland_surface)
{
    assert(xwayland_surface);

    if (xwayland_surface->surface == NULL)
        return false;

    return false;
}

static void e_view_xwayland_configure(struct e_view* view, int lx, int ly, int width, int height)
{
    assert(view);

    struct e_xwayland_view* xwayland_view = view->data;

    view->pending = (struct wlr_box){lx, ly, width, height};

    //if configure is already scheduled, commit will start next one
    if (!xwayland_surface_geo_configure_is_scheduled(xwayland_view->xwayland_surface))
        wlr_xwayland_surface_configure(xwayland_view->xwayland_surface, lx, ly, (uint16_t)width, (uint16_t)height);

    //TODO: according to labwc, if the xwayland surface is offscreen it may not send a commit event
    // and thus not move the view as wait we for a commit that never happens. In these cases we should move it immediately.
    // Views aren't usually offscreen though, so this shouldn't be that big of a deal.
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

    //awesome if-statement
    if (wlr_xwayland_surface_has_window_type(xwayland_surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DIALOG)
        || wlr_xwayland_surface_has_window_type(xwayland_surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DROPDOWN_MENU)
        || wlr_xwayland_surface_has_window_type(xwayland_surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_POPUP_MENU)
        || wlr_xwayland_surface_has_window_type(xwayland_surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_TOOLTIP)
        || wlr_xwayland_surface_has_window_type(xwayland_surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_SPLASH)
        || wlr_xwayland_surface_has_window_type(xwayland_surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_NOTIFICATION)
        || wlr_xwayland_surface_has_window_type(xwayland_surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_MENU)
        || wlr_xwayland_surface_has_window_type(xwayland_surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_COMBO))
    {
        return true;
    }

    return false;
}

static void e_view_xwayland_send_close(struct e_view* view)
{
    assert(view && view->data);

    struct e_xwayland_view* xwayland_view = view->data;

    wlr_xwayland_surface_close(xwayland_view->xwayland_surface);
}

static const struct e_view_impl view_xwayland_implementation = {
    .get_size_hints = e_view_xwayland_get_size_hints,

    .set_tiled = e_view_xwayland_set_tiled,
    
    .set_activated = e_view_xwayland_set_activated,
    .set_fullscreen = e_view_xwayland_set_fullscreen,

    .configure = e_view_xwayland_configure,
    .create_content_tree = e_view_xwayland_create_content_tree,
    .wants_floating = e_view_xwayland_wants_floating,
    .send_close = e_view_xwayland_send_close,
};

// Creates new xwayland view for server.
// Returns NULL on fail.
struct e_xwayland_view* e_xwayland_view_create(struct e_server* server, struct wlr_xwayland_surface* xwayland_surface)
{
    assert(server && xwayland_surface);
    
    struct e_xwayland_view* xwayland_view = calloc(1, sizeof(*xwayland_view));

    if (xwayland_view == NULL)
    {
        e_log_error("e_xwayland_view_create: failed to allocate xwayland_view");
        return NULL;
    }

    xwayland_view->xwayland_surface = xwayland_surface;

    e_view_init(&xwayland_view->base, server, E_VIEW_XWAYLAND, xwayland_view, &view_xwayland_implementation);

    xwayland_view->base.title = xwayland_surface->title;

    // events

    SIGNAL_CONNECT(xwayland_surface->events.set_title, xwayland_view->set_title, e_xwayland_view_set_title);
    SIGNAL_CONNECT(xwayland_surface->events.map_request, xwayland_view->map_request, e_xwayland_view_map_request);
    
    SIGNAL_CONNECT(xwayland_surface->events.request_fullscreen, xwayland_view->request_fullscreen, e_xwayland_view_request_fullscreen);
    SIGNAL_CONNECT(xwayland_surface->events.request_maximize, xwayland_view->request_maximize, e_xwayland_view_request_maximize);
    SIGNAL_CONNECT(xwayland_surface->events.request_configure, xwayland_view->request_configure, e_xwayland_view_request_configure);
    SIGNAL_CONNECT(xwayland_surface->events.request_move, xwayland_view->request_move, e_xwayland_view_request_move);
    SIGNAL_CONNECT(xwayland_surface->events.request_resize, xwayland_view->request_resize, e_xwayland_view_request_resize);

    SIGNAL_CONNECT(xwayland_surface->events.associate, xwayland_view->associate, e_xwayland_view_associate);
    SIGNAL_CONNECT(xwayland_surface->events.dissociate, xwayland_view->dissociate, e_xwayland_view_dissociate);
    
    SIGNAL_CONNECT(xwayland_surface->events.destroy, xwayland_view->destroy, e_xwayland_view_destroy);

    return xwayland_view;
}
