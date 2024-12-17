#include "types/windows/toplevel_window.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

#include "types/server.h"

//make window and its tree visible
static void e_toplevel_window_map(struct wl_listener* listener, void* data)
{
    //TODO: implement e_toplevel_window_map
}

//make window and its tree invisible
static void e_toplevel_window_unmap(struct wl_listener* listener, void* data)
{
    //TODO: implement e_toplevel_window_unmap
}

//new window frame
static void e_toplevel_window_commit(struct wl_listener* listener, void* data)
{
    //TODO: implement e_toplevel_window_commit
}

static void e_toplevel_window_destroy(struct wl_listener* listener, void* data)
{
    //TODO: implement e_toplevel_window_destroy
    //TODO: destroy window linked list and listener linked lists

    //free(toplevel_window);
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
    xdg_toplevel->base->data = toplevel_window->scene_tree;

    //TODO: implement map, unmap, commit and destroy event

    return toplevel_window;
}