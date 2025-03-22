#include "desktop/layers/layer_surface.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "desktop/desktop.h"
#include "desktop/tree/node.h"
#include "desktop/layers/layer_shell.h"
#include "desktop/layers/layer_popup.h"

#include "input/seat.h"

#include "output.h"

#include "util/log.h"

//new xdg_popup
static void e_layer_surface_new_popup(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, commit);
    struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->scene_layer_surface_v1->layer_surface;
    struct wlr_xdg_popup* xdg_popup = data;

    e_layer_popup_create(xdg_popup, wlr_layer_surface_v1);   
}

static struct wlr_scene_tree* e_output_get_layer_tree(struct e_output* output, enum zwlr_layer_shell_v1_layer layer)
{
    switch(layer)
    {
        case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
            return output->layers.background;
        case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
            return output->layers.bottom;
        case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
            return output->layers.top;
        case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
            return output->layers.overlay;
        default:
            e_log_error("no layer tree for layer");
            return NULL;
    }
}

//adds this layer surface to the layer surface it wants
static void e_layer_surface_add_to_desired_layer(struct e_layer_surface* layer_surface, struct wlr_layer_surface_v1* wlr_layer_surface_v1)
{
    struct wlr_scene_tree* layer_tree = e_output_get_layer_tree(layer_surface->output, wlr_layer_surface_v1->current.layer);

    if (layer_tree == NULL)
    {
        e_log_error("no layer tree for layer");
        abort();
    }

    layer_surface->scene_layer_surface_v1 = wlr_scene_layer_surface_v1_create(layer_tree, wlr_layer_surface_v1);

    layer_surface->scene_tree = layer_surface->scene_layer_surface_v1->tree;
    e_node_desc_create(&layer_surface->scene_tree->node, E_NODE_DESC_LAYER_SURFACE, layer_surface);
}

//if this layer surface requests exclusive focus, and is on a higher layer than or a layer equal to the current focused exclusive layer surface
static bool e_layer_surface_should_get_exclusive_focus(struct e_layer_surface* layer_surface)
{
    struct e_seat* seat = layer_surface->desktop->seat;

    struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->scene_layer_surface_v1->layer_surface;

    //can't get exclusive focus if it doesn't request exclusive focus, duh
    if (wlr_layer_surface_v1->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
        return false;

    if (seat->focus_surface == NULL)
        return true;

    struct wlr_layer_surface_v1* focused_layer_surface_v1 = wlr_layer_surface_v1_try_from_wlr_surface(seat->focus_surface);

    //Should get exclusive focus, current focused surface is not a layer surface, or on lower layer, or doesn't request exclusive focus
    return (focused_layer_surface_v1 == NULL || wlr_layer_surface_v1->current.layer >= focused_layer_surface_v1->current.layer || focused_layer_surface_v1->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
}

static void e_layer_surface_commit(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, commit);
    struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->scene_layer_surface_v1->layer_surface;

    bool update_arrangement = false;

    //configure on initial commit
    if (wlr_layer_surface_v1->initial_commit)
    {
        wl_list_insert(&layer_surface->desktop->layer_surfaces, &layer_surface->link);

        update_arrangement = true;
    }

    //committed layer
    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_LAYER)
    {
        struct wlr_scene_tree* layer_tree = e_output_get_layer_tree(layer_surface->output, wlr_layer_surface_v1->current.layer);

        if (layer_tree != NULL)
            wlr_scene_node_reparent(&layer_surface->scene_tree->node, layer_tree);

        e_log_info("layer surface committed layer");
        update_arrangement = true;
    }

    //committed keyboard interactivity
    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_KEYBOARD_INTERACTIVITY)
    {
        struct e_seat* seat = layer_surface->desktop->seat;

        //give exclusive focus if requested and allowed and doesn't have focus
        if (e_layer_surface_should_get_exclusive_focus(layer_surface) && !e_seat_has_focus(seat, wlr_layer_surface_v1->surface))
            e_seat_set_focus(seat, wlr_layer_surface_v1->surface, true);
        //clear focus if layer surface no longer wants focus and has focus
        else if (wlr_layer_surface_v1->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE && e_seat_has_focus(seat, wlr_layer_surface_v1->surface))
            e_seat_clear_focus(seat);

        e_log_info("layer surface committed keyboard interactivity");
    }

    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_DESIRED_SIZE)
    {
        e_log_info("layer surface committed desired size");
        update_arrangement = true;
    }

    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_ANCHOR)
    {
        e_log_info("layer surface committed anchor");
        update_arrangement = true;
    }

    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_EXCLUSIVE_ZONE)
    {
        e_log_info("layer surface committed exclusive zone");
        update_arrangement = true;
    }

    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_MARGIN)
    {
        e_log_info("layer surface committed margin");
        update_arrangement = true;
    }

    if (update_arrangement)
        e_desktop_arrange_layer(layer_surface->desktop, layer_surface->output->wlr_output, e_layer_surface_get_layer(layer_surface));
}

static void e_layer_surface_unmap(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* unmapped_layer_surface = wl_container_of(listener, unmapped_layer_surface, unmap);

    wl_list_remove(&unmapped_layer_surface->link);
    e_desktop_arrange_layer(unmapped_layer_surface->desktop, e_layer_surface_get_wlr_output(unmapped_layer_surface), e_layer_surface_get_layer(unmapped_layer_surface));

    //get next topmost layer surface that requests exclusive focus, and focus on it

    struct e_layer_surface* next_layer_surface = e_desktop_get_exclusive_topmost_layer_surface(unmapped_layer_surface->desktop);

    if (next_layer_surface != NULL)
        e_seat_set_focus(next_layer_surface->desktop->seat, next_layer_surface->scene_layer_surface_v1->layer_surface->surface, true);
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

struct e_layer_surface* e_layer_surface_create(struct e_desktop* desktop, struct wlr_layer_surface_v1* wlr_layer_surface_v1)
{
    assert(desktop && wlr_layer_surface_v1 && wlr_layer_surface_v1->output);

    struct e_layer_surface* layer_surface = calloc(1, sizeof(struct e_layer_surface));
    layer_surface->desktop = desktop;

    layer_surface->output = wlr_layer_surface_v1->output->data;
    
    wl_list_init(&layer_surface->link);

    e_layer_surface_add_to_desired_layer(layer_surface, wlr_layer_surface_v1);
    
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