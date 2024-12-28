#include "desktop/layer_surface.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/util/box.h>

#include "desktop/scene.h"
#include "server.h"

//TODO: keyboard focus

static void e_layer_surface_commit(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, commit);
    struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->wlr_scene_layer_surface_v1->layer_surface;

    //TODO: arrange layout of layers on initial commit
    //configure on initial commit
    if (wlr_layer_surface_v1->initial_commit)
        wlr_layer_surface_v1_configure(wlr_layer_surface_v1, wlr_layer_surface_v1->current.desired_width, wlr_layer_surface_v1->current.desired_height);

}

static void e_layer_surface_destroy(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, destroy);

    wl_list_remove(&layer_surface->commit.link);
    wl_list_remove(&layer_surface->destroy.link);

    free(layer_surface);
}

struct e_layer_surface* e_layer_surface_create(struct e_server* server, struct wlr_layer_surface_v1 *wlr_layer_surface_v1)
{
    struct e_layer_surface* layer_surface = calloc(1, sizeof(struct e_layer_surface));
    layer_surface->server = server;
    
    //TODO: currently always in overlay layer, allow for other layers aswell
    layer_surface->wlr_scene_layer_surface_v1 = wlr_scene_layer_surface_v1_create(server->scene->layers.overlay, wlr_layer_surface_v1);
    layer_surface->scene_tree = layer_surface->wlr_scene_layer_surface_v1->tree;
    
    //allows popups to attach themselves this layer surface's scene tree
    wlr_layer_surface_v1->data = layer_surface->scene_tree;

    //events

    layer_surface->commit.notify = e_layer_surface_commit;
    wl_signal_add(&wlr_layer_surface_v1->surface->events.commit, &layer_surface->commit);

    layer_surface->destroy.notify = e_layer_surface_destroy;
    wl_signal_add(&wlr_layer_surface_v1->events.destroy, &layer_surface->destroy);

    return layer_surface;
}