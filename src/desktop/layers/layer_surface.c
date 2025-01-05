#include "desktop/layers/layer_surface.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/util/box.h>

#include "desktop/layers/layer_popup.h"
#include "desktop/xdg_shell.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "input/seat.h"

#include "desktop/layers/layer_shell.h"
#include "desktop/layers/layer_popup.h"
#include "desktop/scene.h"
#include "server.h"

//TODO: pointer focus everywhere for exclusive focused layer surfaces

//new xdg_popup
static void e_layer_surface_new_popup(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, commit);
    struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->scene_layer_surface_v1->layer_surface;
    struct wlr_xdg_popup* xdg_popup = data;

    e_layer_popup_create(xdg_popup, wlr_layer_surface_v1);   
}

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
    layer_surface->scene_tree->node.data = layer_surface;
}

//if this layer surface requests exclusive focus, and is on a higher layer than or a layer equal to the current focused layer surface
static bool e_layer_surface_should_get_exclusive_focus(struct e_layer_surface* layer_surface)
{
    struct e_seat* seat = layer_surface->server->input_manager->seat;

    struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->scene_layer_surface_v1->layer_surface;

    //can't get exclusive focus if it doesn't request exclusive focus, duh
    if (wlr_layer_surface_v1->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
        return false;

    struct wlr_layer_surface_v1* focused_layer_surface_v1 = wlr_layer_surface_v1_try_from_wlr_surface(seat->focus_surface);

    return (focused_layer_surface_v1 == NULL || wlr_layer_surface_v1->current.layer >= focused_layer_surface_v1->current.layer);
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

        //give exclusive focus is requested and allowed
        if (e_layer_surface_should_get_exclusive_focus(layer_surface))
            e_seat_set_focus(layer_surface->server->input_manager->seat, wlr_layer_surface_v1->surface, true);
    }
}

static void e_layer_surface_unmap(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* unmapped_layer_surface = wl_container_of(listener, unmapped_layer_surface, unmap);
    struct e_layer_shell* layer_shell = unmapped_layer_surface->server->layer_shell;

    wl_list_remove(&unmapped_layer_surface->link);
    e_layer_shell_arrange_layer(layer_shell, e_layer_surface_get_wlr_output(unmapped_layer_surface), e_layer_surface_get_layer(unmapped_layer_surface));

    //get next topmost layer surface that requests exclusive focus, and focus on it

    struct e_layer_surface* next_layer_surface = e_layer_shell_get_exclusive_topmost_layer_surface(layer_shell);

    if (next_layer_surface != NULL)
        e_seat_set_focus(next_layer_surface->server->input_manager->seat, next_layer_surface->scene_layer_surface_v1->layer_surface->surface, true);
}

static void e_layer_surface_destroy(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, destroy);

    wl_list_remove(&layer_surface->new_popup.link);
    wl_list_remove(&layer_surface->commit.link);
    wl_list_remove(&layer_surface->unmap.link);
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

    layer_surface->new_popup.notify = e_layer_surface_new_popup;
    wl_signal_add(&wlr_layer_surface_v1->events.new_popup, &layer_surface->new_popup);

    layer_surface->commit.notify = e_layer_surface_commit;
    wl_signal_add(&wlr_layer_surface_v1->surface->events.commit, &layer_surface->commit);

    layer_surface->unmap.notify = e_layer_surface_unmap;
    wl_signal_add(&wlr_layer_surface_v1->surface->events.unmap, &layer_surface->unmap);

    layer_surface->destroy.notify = e_layer_surface_destroy;
    wl_signal_add(&wlr_layer_surface_v1->events.destroy, &layer_surface->destroy);

    return layer_surface;
}

//configures an e_layer_surface's layout, updates remaining area
void e_layer_surface_configure(struct e_layer_surface* layer_surface, struct wlr_box* full_area, struct wlr_box* remaining_area)
{
    //struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->scene_layer_surface_v1->layer_surface;

    //TODO: account for committed changes

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