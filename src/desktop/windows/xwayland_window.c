#include "desktop/windows/xwayland_window.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>
#include <wlr/xwayland.h>

#include "desktop/desktop.h"
#include "desktop/tree/node.h"
#include "desktop/windows/window.h"

#include "input/cursor.h"

#include "util/log.h"

static void e_xwayland_window_map(struct wl_listener* listener, void* data)
{
    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, map);

    e_window_set_tiled(&xwayland_window->base, !xwayland_window->xwayland_surface->override_redirect);
    e_window_map(&xwayland_window->base);
}

static void e_xwayland_window_unmap(struct wl_listener* listener, void* data)
{
    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, unmap);

    e_window_unmap(&xwayland_window->base);
}

static void e_xwayland_window_commit(struct wl_listener* listener, void* data)
{
    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, commit);

    xwayland_window->base.current.width = xwayland_window->xwayland_surface->width;
    xwayland_window->base.current.height = xwayland_window->xwayland_surface->height;

    //TODO: implement e_xwayland_window_commit
}

static void e_xwayland_window_request_maximize(struct wl_listener* listener, void* data)
{
    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, request_configure);
    
    //only maximize floating windows
    if (!xwayland_window->base.tiled)
        e_window_maximize(&xwayland_window->base);
}

static void e_xwayland_window_request_configure(struct wl_listener* listener, void* data)
{
    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, request_configure);
    struct wlr_xwayland_surface_configure_event* event = data;
    
    if (!xwayland_window->base.tiled)
    {
        e_window_set_position(&xwayland_window->base, event->x, event->y);
        e_window_set_size(&xwayland_window->base, event->width, event->height);
    }
}

static void e_xwayland_window_request_move(struct wl_listener* listener, void* data)
{
    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, request_move);
    
    struct e_desktop* desktop = xwayland_window->base.desktop;
    e_cursor_start_window_move(desktop->seat->cursor, &xwayland_window->base);
}

static void e_xwayland_window_request_resize(struct wl_listener* listener, void* data)
{
    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, request_resize);
    struct wlr_xwayland_resize_event* event = data;
    
    struct e_desktop* desktop = xwayland_window->base.desktop;
    e_cursor_start_window_resize(desktop->seat->cursor, &xwayland_window->base, event->edges);
}

static void e_xwayland_window_set_title(struct wl_listener* listener, void* data)
{
    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, set_title);

    xwayland_window->base.title = xwayland_window->xwayland_surface->title;
}

//surface becomes valid, like me!
static void e_xwayland_window_associate(struct wl_listener* listener, void* data)
{
    //surface is valid
    e_log_info("associate: xwayland window surface is now valid");

    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, associate);

    xwayland_window->base.surface = xwayland_window->xwayland_surface->surface;

    // events

    xwayland_window->map.notify = e_xwayland_window_map;
    wl_signal_add(&xwayland_window->xwayland_surface->surface->events.map, &xwayland_window->map);

    xwayland_window->unmap.notify = e_xwayland_window_unmap;
    wl_signal_add(&xwayland_window->xwayland_surface->surface->events.unmap, &xwayland_window->unmap);

    xwayland_window->commit.notify = e_xwayland_window_commit;
    wl_signal_add(&xwayland_window->xwayland_surface->surface->events.commit, &xwayland_window->commit);

    // requests

    xwayland_window->request_maximize.notify = e_xwayland_window_request_maximize;
    wl_signal_add(&xwayland_window->xwayland_surface->events.request_maximize, &xwayland_window->request_maximize);

    xwayland_window->request_configure.notify = e_xwayland_window_request_configure;
    wl_signal_add(&xwayland_window->xwayland_surface->events.request_configure, &xwayland_window->request_configure);

    xwayland_window->request_move.notify = e_xwayland_window_request_move;
    wl_signal_add(&xwayland_window->xwayland_surface->events.request_move, &xwayland_window->request_move);
    
    xwayland_window->request_resize.notify = e_xwayland_window_request_resize;
    wl_signal_add(&xwayland_window->xwayland_surface->events.request_resize, &xwayland_window->request_resize);

    // other events

    xwayland_window->set_title.notify = e_xwayland_window_set_title;
    wl_signal_add(&xwayland_window->xwayland_surface->events.set_title, &xwayland_window->set_title);

    //add surface & subsurfaces to scene by creating a subsurface tree
    struct e_desktop* desktop = xwayland_window->base.desktop;
    xwayland_window->base.tree = wlr_scene_subsurface_tree_create(desktop->pending, xwayland_window->xwayland_surface->surface);
    e_node_desc_create(&xwayland_window->base.tree->node, E_NODE_DESC_WINDOW, &xwayland_window->base);

    e_window_create_container_tree(&xwayland_window->base, desktop->pending);
}

//surface becomes invalid
static void e_xwayland_window_dissociate(struct wl_listener* listener, void* data)
{
    //surface is now invalid
    e_log_info("dissociate: xwayland window surface is now invalid");

    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, dissociate);

    xwayland_window->base.surface = NULL;

    wl_list_remove(&xwayland_window->map.link);
    wl_list_remove(&xwayland_window->unmap.link);
    wl_list_remove(&xwayland_window->commit.link);

    wl_list_remove(&xwayland_window->request_maximize.link);
    wl_list_remove(&xwayland_window->request_configure.link);
    wl_list_remove(&xwayland_window->request_move.link);
    wl_list_remove(&xwayland_window->request_resize.link);

    wl_list_remove(&xwayland_window->set_title.link);

    //remove from scene
    e_window_destroy_container_tree(&xwayland_window->base);
}

//destruction...
static void e_xwayland_window_destroy(struct wl_listener* listener, void* data)
{
    //remove associate & dissociate listener lists & free

    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, destroy);

    wl_list_remove(&xwayland_window->associate.link);
    wl_list_remove(&xwayland_window->dissociate.link);
    wl_list_remove(&xwayland_window->destroy.link);

    e_window_fini(&xwayland_window->base);

    free(xwayland_window);
}

static uint32_t e_window_xwayland_configure(struct e_window* window, int lx, int ly, int width, int height)
{
    assert(window && window->tree);

    wlr_scene_node_set_position(&window->tree->node, lx, ly);

    struct e_xwayland_window* xwayland_window = window->data;

    wlr_xwayland_surface_configure(xwayland_window->xwayland_surface, lx, ly, (uint16_t)width, (uint16_t)height);

    return 0;
}

static void e_window_xwayland_send_close(struct e_window* window)
{
    assert(window && window->data);

    struct e_xwayland_window* xwayland_window = window->data;

    wlr_xwayland_surface_close(xwayland_window->xwayland_surface);
}

//creates new xwayland window inside server
struct e_xwayland_window* e_xwayland_window_create(struct e_desktop* desktop, struct wlr_xwayland_surface* xwayland_surface)
{
    assert(desktop && xwayland_surface);
    
    e_log_info("new xwayland window");

    struct e_xwayland_window* xwayland_window = calloc(1, sizeof(*xwayland_window));

    if (xwayland_window == NULL)
    {
        e_log_error("failed to allocate xwayland_window");
        return NULL;
    }

    xwayland_window->xwayland_surface = xwayland_surface;

    e_window_init(&xwayland_window->base, desktop, E_WINDOW_XWAYLAND);
    xwayland_window->base.data = xwayland_window;

    xwayland_window->base.title = xwayland_surface->title;

    xwayland_window->base.implementation.configure = e_window_xwayland_configure;
    xwayland_window->base.implementation.send_close = e_window_xwayland_send_close;

    // events

    xwayland_window->associate.notify = e_xwayland_window_associate;
    wl_signal_add(&xwayland_surface->events.associate, &xwayland_window->associate);
    
    xwayland_window->dissociate.notify = e_xwayland_window_dissociate;
    wl_signal_add(&xwayland_surface->events.dissociate, &xwayland_window->dissociate);

    xwayland_window->destroy.notify = e_xwayland_window_destroy;
    wl_signal_add(&xwayland_surface->events.destroy, &xwayland_window->destroy);

    return xwayland_window;
}