#include "desktop/windows/toplevel_window.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

#include "desktop/windows/window.h"
#include "desktop/scene.h"
#include "server.h"

//surface is ready to be displayed
static void e_toplevel_window_map(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, map);

    e_window_map(toplevel_window->base);
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
        //0x0 size to let windows configure their size themselves    
        wlr_xdg_toplevel_set_size(toplevel_window->xdg_toplevel, 0, 0);
    }
}

//xdg_toplevel got destroyed
static void e_toplevel_window_destroy(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, destroy);

    wl_list_remove(&toplevel_window->map.link);
    wl_list_remove(&toplevel_window->unmap.link);
    wl_list_remove(&toplevel_window->commit.link);
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

    //surface map event
    toplevel_window->map.notify = e_toplevel_window_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel_window->map);

    //surface unmap event
    toplevel_window->unmap.notify = e_toplevel_window_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel_window->unmap);

    //surface commit event
    toplevel_window->commit.notify = e_toplevel_window_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel_window->commit);
    
    //window destroy event
    toplevel_window->destroy.notify = e_toplevel_window_destroy;
    wl_signal_add(&xdg_toplevel->events.destroy, &toplevel_window->destroy);
    
    //TODO: request resize, fullscreen, ... events

    return toplevel_window;
}