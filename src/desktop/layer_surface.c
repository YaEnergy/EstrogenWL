#include "desktop/layer_surface.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/util/box.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "desktop/layer_shell.h"
#include "desktop/scene.h"
#include "server.h"

//TODO: keyboard focus

//adds this layer surface to the layer surface it wants
static void e_layer_surface_add_to_desired_layer(struct e_scene* scene, struct e_layer_surface* layer_surface, struct wlr_layer_surface_v1* wlr_layer_surface_v1)
{
    switch (wlr_layer_surface_v1->current.layer) 
    {
        case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
            layer_surface->scene_layer_surface_v1 = wlr_scene_layer_surface_v1_create(scene->layers.background, wlr_layer_surface_v1);
            break;
        case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
            layer_surface->scene_layer_surface_v1 = wlr_scene_layer_surface_v1_create(scene->layers.bottom, wlr_layer_surface_v1);
            break;
        case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
            layer_surface->scene_layer_surface_v1 = wlr_scene_layer_surface_v1_create(scene->layers.top, wlr_layer_surface_v1);
            break;
        case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
            layer_surface->scene_layer_surface_v1 = wlr_scene_layer_surface_v1_create(scene->layers.overlay, wlr_layer_surface_v1);
            break;
    }

    layer_surface->scene_tree = layer_surface->scene_layer_surface_v1->tree;
}

static void e_layer_surface_commit(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, commit);
    struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->scene_layer_surface_v1->layer_surface;

    //configure on initial commit
    if (wlr_layer_surface_v1->initial_commit)
    {
        wl_list_insert(&layer_surface->server->layer_shell->layer_surfaces, &layer_surface->link);  
        e_layer_shell_arrange_layer(layer_surface->server->layer_shell, e_layer_surface_get_wlr_output(layer_surface), e_layer_surface_get_layer(layer_surface)); 
    }
}

static void e_layer_surface_destroy(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, destroy);

    wl_list_remove(&layer_surface->link);
    wl_list_remove(&layer_surface->commit.link);
    wl_list_remove(&layer_surface->destroy.link);

    free(layer_surface);
}

struct e_layer_surface* e_layer_surface_create(struct e_server* server, struct wlr_layer_surface_v1* wlr_layer_surface_v1)
{
    struct e_layer_surface* layer_surface = calloc(1, sizeof(struct e_layer_surface));
    layer_surface->server = server;
    
    wl_list_init(&layer_surface->link);

    e_layer_surface_add_to_desired_layer(server->scene, layer_surface, wlr_layer_surface_v1);
    
    //allows popups to attach themselves this layer surface's scene tree
    wlr_layer_surface_v1->data = layer_surface->scene_tree;

    //events

    layer_surface->commit.notify = e_layer_surface_commit;
    wl_signal_add(&wlr_layer_surface_v1->surface->events.commit, &layer_surface->commit);

    layer_surface->destroy.notify = e_layer_surface_destroy;
    wl_signal_add(&wlr_layer_surface_v1->events.destroy, &layer_surface->destroy);

    return layer_surface;
}

//configures an e_layer_surface's layout, updates remaining area
void e_layer_surface_configure(struct e_layer_surface* layer_surface, struct wlr_box* full_area, struct wlr_box* remaining_area)
{
    struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->scene_layer_surface_v1->layer_surface;

    //TODO: account for committed changes
    //enum wlr_layer_surface_v1_state_field

    //desired size committed?
    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_DESIRED_SIZE)
    {

    }

    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_ANCHOR)
    {
        
    }

    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_EXCLUSIVE_ZONE)
    {
        
    }

    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_MARGIN)
    {
        
    }

    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_KEYBOARD_INTERACTIVITY)
    {
        
    }

    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_LAYER)
    {
        
    }

    //updates remaining area
    wlr_scene_layer_surface_v1_configure(layer_surface->scene_layer_surface_v1, full_area, remaining_area);
}

enum zwlr_layer_shell_v1_layer e_layer_surface_get_layer(struct e_layer_surface* layer_surface)
{
    return layer_surface->scene_layer_surface_v1->layer_surface->current.layer;
}

struct wlr_output* e_layer_surface_get_wlr_output(struct e_layer_surface* layer_surface)
{
    return layer_surface->scene_layer_surface_v1->layer_surface->output;
}