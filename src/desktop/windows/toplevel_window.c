#include "desktop/windows/toplevel_window.h"

#include <stdlib.h>
#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

#include "desktop/windows/window.h"
#include "desktop/scene.h"
#include "input/cursor.h"
#include "server.h"
#include "util/log.h"
#include "wm.h"

#include <wlr/util/edges.h>

static bool e_toplevel_window_wants_floating(struct e_toplevel_window* toplevel_window)
{
    struct wlr_xdg_toplevel* xdg_toplevel = toplevel_window->xdg_toplevel;
    return (xdg_toplevel->current.min_width > 0 || xdg_toplevel->current.min_height > 0) && (xdg_toplevel->current.min_width == xdg_toplevel->current.max_width || xdg_toplevel->current.min_height == xdg_toplevel->current.max_height);
}

//surface is ready to be displayed
static void e_toplevel_window_map(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, map);

    e_window_map(toplevel_window->base);

    //TODO: handle toplevel_window->xdg_toplevel->requested if surface is mapped
}

//surface no longer wants to be displayed
static void e_toplevel_window_unmap(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, unmap);

    e_window_unmap(toplevel_window->base);
}

//new surface state got committed
static void e_toplevel_window_commit(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, commit);
        
    if (toplevel_window->xdg_toplevel->base->initial_commit)
    {
        if (e_toplevel_window_wants_floating(toplevel_window))
        {
            //0x0 size to let windows configure their size themselves, instead of forcing min or max size
            e_window_set_tiled(toplevel_window->base, false);
            e_window_set_size(toplevel_window->base, 0, 0);
            e_log_info("toplevel window wants floating");
        }
        else
        {
            e_window_set_tiled(toplevel_window->base, true);
            e_tile_windows(toplevel_window->base->server);
        }
    }

    //TODO: handle toplevel_window->xdg_toplevel->requested if surface is mapped
}

static void e_toplevel_window_request_fullscreen(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, request_fullscreen);

    //TODO: implement fullscreen

    //send empty configure to conform to xdg shell protocol if xdg surface is init
    if (toplevel_window->xdg_toplevel->base->initialized)
        wlr_xdg_surface_schedule_configure(toplevel_window->xdg_toplevel->base);
}

static void e_toplevel_window_request_maximize(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, request_maximize);

    //TODO: implement maximalization

    //send empty configure to conform to xdg shell protocol if xdg surface is init
    if (toplevel_window->xdg_toplevel->base->initialized)
        wlr_xdg_surface_schedule_configure(toplevel_window->xdg_toplevel->base);
}

static void e_toplevel_window_request_move(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, request_move);
    //struct wlr_xdg_toplevel_move_event* event = data;

    //TODO: any client can request this, verify button serials
    
    struct e_server* server = toplevel_window->base->server;
    e_cursor_start_window_move(server->input_manager->cursor, toplevel_window->base);
}

static void e_toplevel_window_request_resize(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, request_resize);
    struct wlr_xdg_toplevel_resize_event* event = data;

    //TODO: any client can request this, verify buttons serial
    
    struct e_server* server = toplevel_window->base->server;
    e_cursor_start_window_resize(server->input_manager->cursor, toplevel_window->base, event->edges);
}

//xdg_toplevel got destroyed
static void e_toplevel_window_destroy(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, destroy);

    wl_list_remove(&toplevel_window->map.link);
    wl_list_remove(&toplevel_window->unmap.link);
    wl_list_remove(&toplevel_window->commit.link);

    wl_list_remove(&toplevel_window->request_fullscreen.link);
    wl_list_remove(&toplevel_window->request_maximize.link);
    wl_list_remove(&toplevel_window->request_move.link);
    wl_list_remove(&toplevel_window->request_resize.link);

    wl_list_remove(&toplevel_window->destroy.link);

    e_window_destroy(toplevel_window->base);

    free(toplevel_window);
}

struct e_toplevel_window* e_toplevel_window_create(struct e_server* server, struct wlr_xdg_toplevel* xdg_toplevel)
{
    struct e_toplevel_window* toplevel_window = calloc(1, sizeof(struct e_toplevel_window));

    //give pointer to xdg toplevel
    toplevel_window->xdg_toplevel = xdg_toplevel;

    struct e_window* window = e_window_create(server, E_WINDOW_TOPLEVEL);
    window->toplevel_window = toplevel_window;
    toplevel_window->base = window;

    e_window_init_xdg_scene_tree(window, server->scene->layers.tiling, xdg_toplevel->base);

    // events

    // surface events

    toplevel_window->map.notify = e_toplevel_window_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel_window->map);

    toplevel_window->unmap.notify = e_toplevel_window_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel_window->unmap);

    toplevel_window->commit.notify = e_toplevel_window_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel_window->commit);

    // xdg toplevel events

    toplevel_window->request_fullscreen.notify = e_toplevel_window_request_fullscreen;
    wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel_window->request_fullscreen);

    toplevel_window->request_maximize.notify = e_toplevel_window_request_maximize;
    wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel_window->request_maximize);

    toplevel_window->request_move.notify = e_toplevel_window_request_move;
    wl_signal_add(&xdg_toplevel->events.request_move, &toplevel_window->request_move);

    toplevel_window->request_resize.notify = e_toplevel_window_request_resize;
    wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel_window->request_resize);

    //window destroy event
    toplevel_window->destroy.notify = e_toplevel_window_destroy;
    wl_signal_add(&xdg_toplevel->events.destroy, &toplevel_window->destroy);
    
    //TODO: request resize, fullscreen, ... events

    return toplevel_window;
}