#include "windows/toplevel_window.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

#include "input/input_manager.h"
#include "input/seat.h"
#include "windows/surface.h"
#include "server.h"
#include "wm.h"

//surface is ready to be displayed
static void e_toplevel_window_map(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, map);

    wl_list_insert(&toplevel_window->server->xdg_shell->toplevel_windows, &toplevel_window->link);

    e_tile_toplevel_windows(toplevel_window->server);

    e_seat_set_focus(toplevel_window->server->input_manager->seat, toplevel_window->xdg_toplevel->base->surface);
}

//surface no longer wants to be displayed
static void e_toplevel_window_unmap(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, unmap);

    wl_list_remove(&toplevel_window->link);
    
    e_tile_toplevel_windows(toplevel_window->server);

    //if this window's surface had focus, clear it
    if (e_seat_has_focus(toplevel_window->server->input_manager->seat, toplevel_window->xdg_toplevel->base->surface))
        e_seat_clear_focus(toplevel_window->server->input_manager->seat);
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

    free(toplevel_window);
}

struct e_toplevel_window* e_toplevel_window_create(struct e_server* server, struct wlr_xdg_toplevel* xdg_toplevel)
{
    struct e_toplevel_window* toplevel_window = calloc(1, sizeof(struct e_toplevel_window));

    //give pointer to server and xdg toplevel
    toplevel_window->server = server;
    toplevel_window->xdg_toplevel = xdg_toplevel;

    //create xdg surface for xdg toplevel and window, and set up window scene tree
    toplevel_window->scene_tree = wlr_scene_xdg_surface_create(&toplevel_window->server->scene->tree, xdg_toplevel->base);
    toplevel_window->scene_tree->node.data = toplevel_window;

    //allows popup window scene trees to add themselves to this toplevel window's scene tree
    xdg_toplevel->base->data = toplevel_window->scene_tree;

    wl_list_init(&toplevel_window->link);

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

void e_toplevel_window_set_position(struct e_toplevel_window* window, int x, int y)
{
    wlr_scene_node_set_position(&window->scene_tree->node, x, y);
}

void e_toplevel_window_set_size(struct e_toplevel_window* window, int32_t x, int32_t y)
{
    wlr_xdg_toplevel_set_size(window->xdg_toplevel, x, y);
}

struct e_toplevel_window* e_toplevel_window_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_surface** surface, double* sx, double* sy)
{
    struct wlr_scene_node* snode;
   *surface = e_wlr_surface_at(node, lx, ly, &snode, sx, sy);

    if (snode == NULL || *surface == NULL)
        return NULL;
        
    //keep going up the tree, until our parent is the root of the tree, 
    //which means we'll have found the top window
    while (snode != NULL && snode->parent != NULL && snode->parent->node.parent != NULL)
        snode = &snode->parent->node;

    if (snode == NULL)
        return NULL;

    return snode->data; //e_toplevel_window*
}