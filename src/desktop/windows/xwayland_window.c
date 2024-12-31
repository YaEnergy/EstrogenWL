#include "desktop/windows/xwayland_window.h"

#include <stdlib.h>
#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/xwayland.h>

#include "desktop/windows/window.h"
#include "desktop/scene.h"
#include "desktop/xwayland.h"
#include "input/cursor.h"
#include "server.h"
#include "util/log.h"
#include "wm.h"

#include <wlr/util/edges.h>

static void e_xwayland_window_request_move(struct wl_listener* listener, void* data)
{
    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, request_move);
    
    struct e_server* server = xwayland_window->base->server;
    e_cursor_start_window_move(server->input_manager->cursor, xwayland_window->base);
}

static void e_xwayland_window_request_resize(struct wl_listener* listener, void* data)
{
    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, request_resize);
    struct wlr_xwayland_resize_event* event = data;
    
    struct e_server* server = xwayland_window->base->server;
    e_cursor_start_window_resize(server->input_manager->cursor, xwayland_window->base, event->edges);
}

//surface becomes valid, like me!
static void e_xwayland_window_associate(struct wl_listener* listener, void* data)
{
    //surface is valid
    e_log_info("associate: xwayland window surface is now valid");

    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, associate);

    //TODO: set up xwayland surface listeners
    // events

    // requests

    xwayland_window->request_move.notify = e_xwayland_window_request_move;
    wl_signal_add(&xwayland_window->xwayland_surface->events.request_move, &xwayland_window->request_move);
    
    xwayland_window->request_resize.notify = e_xwayland_window_request_resize;
    wl_signal_add(&xwayland_window->xwayland_surface->events.request_resize, &xwayland_window->request_resize);

    //add surface & subsurfaces to scene
    struct e_scene* scene = xwayland_window->base->server->scene; //one of these is null...
    xwayland_window->base->scene_tree = wlr_scene_subsurface_tree_create(scene->layers.floating, xwayland_window->xwayland_surface->surface);
}

//surface becomes invalid
static void e_xwayland_window_dissociate(struct wl_listener* listener, void* data)
{
    //surface is now invalid
    e_log_info("dissociate: xwayland window surface is now invalid");

    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, dissociate);

    //TODO: remove xwayland surface listener lists

    wl_list_remove(&xwayland_window->request_move.link);
    wl_list_remove(&xwayland_window->request_resize.link);

    //remove from scene
    wlr_scene_node_destroy(&xwayland_window->base->scene_tree->node);
}

//destruction...
static void e_xwayland_window_destroy(struct wl_listener* listener, void* data)
{
    //remove associate & dissociate listener lists & free

    struct e_xwayland_window* xwayland_window = wl_container_of(listener, xwayland_window, destroy);

    wl_list_remove(&xwayland_window->associate.link);
    wl_list_remove(&xwayland_window->dissociate.link);
    wl_list_remove(&xwayland_window->destroy.link);

    e_window_destroy(xwayland_window->base);

    free(xwayland_window);
}

//creates new xwayland window inside server
struct e_xwayland_window* e_xwayland_window_create(struct e_server* server, struct wlr_xwayland_surface* xwayland_surface)
{
    e_log_info("new xwayland window");

    struct e_xwayland_window* xwayland_window = calloc(1, sizeof(struct e_xwayland_window));
    xwayland_window->xwayland_surface = xwayland_surface;

    struct e_window* window = e_window_create(server, E_WINDOW_XWAYLAND);
    window->xwayland_window = xwayland_window;
    xwayland_window->base = window;

    // events

    xwayland_window->associate.notify = e_xwayland_window_associate;
    wl_signal_add(&xwayland_surface->events.associate, &xwayland_window->associate);
    
    xwayland_window->dissociate.notify = e_xwayland_window_dissociate;
    wl_signal_add(&xwayland_surface->events.dissociate, &xwayland_window->dissociate);

    xwayland_window->destroy.notify = e_xwayland_window_destroy;
    wl_signal_add(&xwayland_surface->events.destroy, &xwayland_window->destroy);

    return xwayland_window;
}