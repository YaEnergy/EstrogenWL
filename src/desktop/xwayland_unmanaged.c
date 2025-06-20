#include "desktop/xwayland.h"

#include <assert.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/xwayland.h>

#include "desktop/desktop.h"

#include "util/wl_macros.h"
#include "util/log.h"

// Xwayland surface wants to be configured in a specific way.
static void e_xwayland_unmanaged_request_configure(struct wl_listener* listener, void* data)
{
    struct e_xwayland_unmanaged* unmanaged = wl_container_of(listener, unmanaged, request_configure);
    struct wlr_xwayland_surface_configure_event* event = data;

    #ifdef E_VERBOSE
    e_log_info("unmanaged request configure");
    #endif

    //always respect configure request if unmanaged
    wlr_xwayland_surface_configure(event->surface, event->x, event->y, event->width, event->height);
}

static void e_xwayland_unmanaged_set_geometry(struct wl_listener* listener, void* data)
{
    struct e_xwayland_unmanaged* unmanaged = wl_container_of(listener, unmanaged, set_geometry);

    #ifdef E_VERBOSE
    e_log_info("unmanaged set geometry: pos (%i, %i)", unmanaged->xwayland_surface->x, unmanaged->xwayland_surface->y);
    #endif

    if (unmanaged->tree != NULL && unmanaged->xwayland_surface != NULL)
        wlr_scene_node_set_position(&unmanaged->tree->node, unmanaged->xwayland_surface->x, unmanaged->xwayland_surface->y);
}

// Surface is ready to be displayed.
static void e_xwayland_unmanaged_map(struct wl_listener* listener, void* data)
{
    struct e_xwayland_unmanaged* unmanaged = wl_container_of(listener, unmanaged, map);

    struct e_desktop* desktop = unmanaged->desktop;

    SIGNAL_CONNECT(unmanaged->xwayland_surface->events.set_geometry, unmanaged->set_geometry, e_xwayland_unmanaged_set_geometry);

    unmanaged->tree = wlr_scene_subsurface_tree_create(desktop->unmanaged, unmanaged->xwayland_surface->surface);
    wlr_scene_node_set_position(&unmanaged->tree->node, unmanaged->xwayland_surface->x, unmanaged->xwayland_surface->y);
}

// Surface no longer wants to be displayed.
static void e_xwayland_unmanaged_unmap(struct wl_listener* listener, void* data)
{
    struct e_xwayland_unmanaged* unmanaged = wl_container_of(listener, unmanaged, unmap);

    SIGNAL_DISCONNECT(unmanaged->set_geometry);

    wlr_scene_node_destroy(&unmanaged->tree->node);
    unmanaged->tree = NULL;
}

// Surface becomes valid.
static void e_xwayland_unmanaged_associate(struct wl_listener* listener, void* data)
{
    struct e_xwayland_unmanaged* unmanaged = wl_container_of(listener, unmanaged, associate);
    struct wlr_xwayland_surface* xwayland_surface = unmanaged->xwayland_surface;
    struct wlr_surface* surface = xwayland_surface->surface;

    SIGNAL_CONNECT(surface->events.map, unmanaged->map, e_xwayland_unmanaged_map);
    SIGNAL_CONNECT(surface->events.unmap, unmanaged->unmap, e_xwayland_unmanaged_unmap);
}

// Surface becomes invalid.
static void e_xwayland_unmanaged_dissociate(struct wl_listener* listener, void* data)
{
    struct e_xwayland_unmanaged* unmanaged = wl_container_of(listener, unmanaged, dissociate);

    SIGNAL_DISCONNECT(unmanaged->map);
    SIGNAL_DISCONNECT(unmanaged->unmap);
}

// Xwayland surface was destroyed.
static void e_xwayland_unmanaged_destroy(struct wl_listener* listener, void* data)
{
    struct e_xwayland_unmanaged* unmanaged = wl_container_of(listener, unmanaged, destroy);

    SIGNAL_DISCONNECT(unmanaged->request_configure);

    SIGNAL_DISCONNECT(unmanaged->associate);
    SIGNAL_DISCONNECT(unmanaged->dissociate);
    SIGNAL_DISCONNECT(unmanaged->destroy);

    free(unmanaged);
}

// Creates new xwayland unmanaged surface on desktop.
// Returns NULL on fail.
struct e_xwayland_unmanaged* e_xwayland_unmanaged_create(struct e_desktop* desktop, struct wlr_xwayland_surface* xwayland_surface)
{
    assert(desktop && xwayland_surface);

    struct e_xwayland_unmanaged* unmanaged = calloc(1, sizeof(*unmanaged));

    if (unmanaged == NULL)
    {
        e_log_error("e_xwayland_unmanaged_create: failed to alloc e_xwayland_unmanaged");
        return NULL;
    }

    unmanaged->desktop = desktop;
    unmanaged->xwayland_surface = xwayland_surface;
    unmanaged->tree = NULL;

    SIGNAL_CONNECT(xwayland_surface->events.request_configure, unmanaged->request_configure, e_xwayland_unmanaged_request_configure);
    
    SIGNAL_CONNECT(xwayland_surface->events.associate, unmanaged->associate, e_xwayland_unmanaged_associate);
    SIGNAL_CONNECT(xwayland_surface->events.dissociate, unmanaged->dissociate, e_xwayland_unmanaged_dissociate);
    SIGNAL_CONNECT(xwayland_surface->events.destroy, unmanaged->destroy, e_xwayland_unmanaged_destroy);

    return unmanaged;
}
