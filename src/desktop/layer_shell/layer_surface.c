#include "desktop/layer_shell.h"

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
#include "desktop/output.h"
#include "desktop/tree/node.h"

#include "util/log.h"
#include "util/wl_macros.h"

#include "server.h"

// Returns NULL on fail.
static struct e_layer_popup* layer_popup_create(struct wlr_xdg_popup* popup, struct e_layer_surface* layer_surface, struct wlr_scene_tree* parent);

static void layer_popup_handle_new_popup(struct wl_listener* listener, void* data)
{
    struct e_layer_popup* popup = wl_container_of(listener, popup, new_popup);

    struct wlr_xdg_popup* xdg_popup = data;

    e_log_info("layer surface creates new popup");
    layer_popup_create(xdg_popup, popup->layer_surface, popup->tree);
}

static void layer_popup_unconstrain(struct e_layer_popup* popup)
{
    assert(popup);

    if (popup == NULL)
        return;

    struct e_output* output = popup->layer_surface->output;

    struct wlr_box layout_output_box;
    wlr_output_layout_get_box(output->layout, output->wlr_output, &layout_output_box);

    //toplevel layout coords
    int lx, ly;
    wlr_scene_node_coords(&popup->layer_surface->scene_layer_surface_v1->tree->node, &lx, &ly);

    //output geometry relative to toplevel
    //ex: output left border | <- 5 pixels space on the left side: -5 -> | toplevel | <- 200 pixels space on the right side: 200 -> | output right border
    struct wlr_box output_toplevel_space_box = (struct wlr_box)
    {
        .x = layout_output_box.x - lx,
        .y = layout_output_box.y - ly,
        .width = layout_output_box.width,
        .height = layout_output_box.height
    };

    wlr_xdg_popup_unconstrain_from_box(popup->xdg_popup, &output_toplevel_space_box);
}

static void layer_popup_handle_reposition(struct wl_listener* listener, void* data)
{
    struct e_layer_popup* popup = wl_container_of(listener, popup, reposition);

    layer_popup_unconstrain(popup);
}

static void layer_popup_handle_commit(struct wl_listener* listener, void* data)
{
    struct e_layer_popup* popup = wl_container_of(listener, popup, commit);
    
    if (popup->xdg_popup->base->initial_commit)
    {
        layer_popup_unconstrain(popup);
        wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
    }
}

static void layer_popup_handle_destroy(struct wl_listener* listener, void* data)
{
    struct e_layer_popup* popup = wl_container_of(listener, popup, destroy);

    SIGNAL_DISCONNECT(popup->reposition);
    SIGNAL_DISCONNECT(popup->new_popup);
    SIGNAL_DISCONNECT(popup->commit);
    SIGNAL_DISCONNECT(popup->destroy);

    free(popup);
}

// Returns NULL on fail.
static struct e_layer_popup* layer_popup_create(struct wlr_xdg_popup* popup, struct e_layer_surface* layer_surface, struct wlr_scene_tree* parent)
{
    assert(popup && layer_surface);

    if (popup == NULL || layer_surface == NULL)
        return NULL;

    struct e_layer_popup* layer_popup = calloc(1, sizeof(*layer_popup));

    if (layer_popup == NULL)
        return NULL;

    layer_popup->layer_surface = layer_surface;
    layer_popup->xdg_popup = popup;

    //create popup's scene tree, and add popup to scene tree of parent
    layer_popup->tree = wlr_scene_xdg_surface_create(parent, popup->base);
    e_node_desc_create(&layer_popup->tree->node, E_NODE_DESC_LAYER_POPUP, popup);

    SIGNAL_CONNECT(popup->events.reposition, layer_popup->reposition, layer_popup_handle_reposition);
    SIGNAL_CONNECT(popup->base->events.new_popup, layer_popup->new_popup, layer_popup_handle_new_popup);
    SIGNAL_CONNECT(popup->base->surface->events.commit, layer_popup->commit, layer_popup_handle_commit);
    SIGNAL_CONNECT(popup->events.destroy, layer_popup->destroy, layer_popup_handle_destroy);

    return layer_popup;
}

// New layer popup.
static void e_layer_surface_handle_new_popup(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, new_popup);
    struct wlr_xdg_popup* xdg_popup = data;

    e_log_info("layer surface creates new popup");
    layer_popup_create(xdg_popup, layer_surface, layer_surface->popup_tree);
}

// Returns NULL on fail.
static struct wlr_scene_tree* e_output_get_layer_tree(struct e_output* output, enum zwlr_layer_shell_v1_layer layer)
{
    assert(output);

    if (output == NULL)
        return NULL;

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
            e_log_error("e_desktop_get_layer_tree: no layer tree for layer (%i)", layer);
            return NULL;
    }
}

static void e_layer_surface_commit(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, commit);
    struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->scene_layer_surface_v1->layer_surface;

    bool update_arrangement = false;

    //configure on initial commit
    if (wlr_layer_surface_v1->initial_commit)
    {
        struct e_output* output = layer_surface->output;

        struct wlr_box output_full_area = {0, 0, 0, 0};
        wlr_output_layout_get_box(output->layout, output->wlr_output, &output_full_area);

        struct wlr_box output_remaining_area = output->usable_area;

        wlr_scene_layer_surface_v1_configure(layer_surface->scene_layer_surface_v1, &output_full_area, &output_remaining_area);
    }

    //committed layer
    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_LAYER)
    {
        struct wlr_scene_tree* layer_tree = e_output_get_layer_tree(layer_surface->output, wlr_layer_surface_v1->current.layer);

        if (layer_tree != NULL)
            wlr_scene_node_reparent(&layer_surface->scene_layer_surface_v1->tree->node, layer_tree);

        e_log_info("layer surface committed layer");
        update_arrangement = true;
    }

    //committed keyboard interactivity
    if (wlr_layer_surface_v1->current.committed & WLR_LAYER_SURFACE_V1_STATE_KEYBOARD_INTERACTIVITY)
    {
        struct e_server* server = layer_surface->server;

        //give exclusive focus if requested and allowed and doesn't have focus
        if (wlr_layer_surface_v1->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE && server->desktop_state.focused_layer_surface != layer_surface)
            e_desktop_set_focus_layer_surface(server, layer_surface);
        //clear focus if layer surface no longer wants focus and has focus
        else if (wlr_layer_surface_v1->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE && server->desktop_state.focused_layer_surface == layer_surface)
            e_desktop_set_focus_layer_surface(server, NULL);

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
        e_output_arrange(layer_surface->output);
}

// Surface is ready to be displayed.
static void e_layer_surface_map(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, map);

    #if E_VERBOSE
    e_log_info("layer surface mapped!");
    #endif

    wl_list_append(layer_surface->output->layer_surfaces, &layer_surface->link);
    
    e_output_arrange(layer_surface->output);

    wlr_scene_node_set_enabled(&layer_surface->scene_layer_surface_v1->tree->node, true);

    //give focus if requests exclusive focus
    if (layer_surface->scene_layer_surface_v1->layer_surface->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
        e_desktop_set_focus_layer_surface(layer_surface->server, layer_surface);
}

// Surface no longer wants to be displayed.
static void e_layer_surface_unmap(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* unmapped_layer_surface = wl_container_of(listener, unmapped_layer_surface, unmap);

    if (unmapped_layer_surface->server->desktop_state.focused_layer_surface == unmapped_layer_surface)
        e_desktop_set_focus_layer_surface(unmapped_layer_surface->server, NULL);

    wlr_scene_node_set_enabled(&unmapped_layer_surface->scene_layer_surface_v1->tree->node, false);

    wl_list_remove(&unmapped_layer_surface->link);

    if (unmapped_layer_surface->output == NULL)
        return;
    
    e_output_arrange(unmapped_layer_surface->output);

    //get next topmost layer surface that requests exclusive focus, and focus on it

    struct e_layer_surface* next_layer_surface = e_output_get_exclusive_topmost_layer_surface(unmapped_layer_surface->output);

    if (next_layer_surface != NULL)
        e_desktop_set_focus_layer_surface(next_layer_surface->server, next_layer_surface);
}

static void e_layer_surface_handle_node_destroy(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, node_destroy);

    SIGNAL_DISCONNECT(layer_surface->new_popup);

    SIGNAL_DISCONNECT(layer_surface->commit);
    SIGNAL_DISCONNECT(layer_surface->map);
    SIGNAL_DISCONNECT(layer_surface->unmap);

    SIGNAL_DISCONNECT(layer_surface->node_destroy);
    SIGNAL_DISCONNECT(layer_surface->output_destroy);

    free(layer_surface);
}

static void e_layer_surface_handle_output_destroy(struct wl_listener* listener, void* data)
{
    struct e_layer_surface* layer_surface = wl_container_of(listener, layer_surface, output_destroy);

    layer_surface->scene_layer_surface_v1->layer_surface->output = NULL;
    layer_surface->output = NULL;
    wlr_layer_surface_v1_destroy(layer_surface->scene_layer_surface_v1->layer_surface);
}

// Create a layer surface for the given server.
// wlr_layer_surface_v1's output should not be NULL.
// Returns NULL on fail.
struct e_layer_surface* e_layer_surface_create(struct e_server* server, struct wlr_layer_surface_v1* wlr_layer_surface_v1)
{
    assert(server && wlr_layer_surface_v1);

    if (wlr_layer_surface_v1->output == NULL)
    {
        e_log_error("e_layer_surface_create: given wlr_layer_surface_v1 has no output");
        return NULL;
    }

    struct e_output* output = wlr_layer_surface_v1->output->data;

    struct wlr_scene_tree* layer_tree = e_output_get_layer_tree(output, wlr_layer_surface_v1->current.layer);

    if (layer_tree == NULL)
    {
        e_log_error("e_layer_surface_create: no layer tree for layer surface");
        return NULL;
    }

    struct wlr_scene_layer_surface_v1* scene_layer_surface = wlr_scene_layer_surface_v1_create(layer_tree, wlr_layer_surface_v1);

    if (scene_layer_surface == NULL)
    {
        e_log_error("e_layer_surface_create: failed to create scene layer surface");
        return NULL;
    }

    struct e_layer_surface* layer_surface = calloc(1, sizeof(*layer_surface));

    if (layer_surface == NULL)
    {
        e_log_error("e_layer_surface_create: failed to alloc e_layer_surface");
        return NULL;
    }

    layer_surface->server = server;
    layer_surface->output = output;
    layer_surface->popup_tree = wlr_scene_tree_create(output->layer_popup_tree);

    layer_surface->scene_layer_surface_v1 = scene_layer_surface;
    e_node_desc_create(&scene_layer_surface->tree->node, E_NODE_DESC_LAYER_SURFACE, layer_surface);
    wlr_scene_node_set_enabled(&scene_layer_surface->tree->node, false);

    wl_list_init(&layer_surface->link);
    
    wlr_layer_surface_v1->data = layer_surface;

    //events

    SIGNAL_CONNECT(wlr_layer_surface_v1->events.new_popup, layer_surface->new_popup, e_layer_surface_handle_new_popup);

    SIGNAL_CONNECT(wlr_layer_surface_v1->surface->events.commit, layer_surface->commit, e_layer_surface_commit);
    SIGNAL_CONNECT(wlr_layer_surface_v1->surface->events.map, layer_surface->map, e_layer_surface_map);
    SIGNAL_CONNECT(wlr_layer_surface_v1->surface->events.unmap, layer_surface->unmap, e_layer_surface_unmap);

    SIGNAL_CONNECT(scene_layer_surface->tree->node.events.destroy, layer_surface->node_destroy, e_layer_surface_handle_node_destroy);
    SIGNAL_CONNECT(wlr_layer_surface_v1->output->events.destroy, layer_surface->output_destroy, e_layer_surface_handle_output_destroy);

    return layer_surface;
}

// Returns layer surface's requested keyboard interactivity.
enum zwlr_layer_surface_v1_keyboard_interactivity e_layer_surface_get_interactivity(struct e_layer_surface* layer_surface)
{
    assert(layer_surface);

    struct wlr_layer_surface_v1* wlr_layer_surface = layer_surface->scene_layer_surface_v1->layer_surface;

    return wlr_layer_surface->current.keyboard_interactive;
}

// Configures an e_layer_surface's layout, updates remaining area.
void e_layer_surface_configure(struct e_layer_surface* layer_surface, struct wlr_box* full_area, struct wlr_box* remaining_area)
{
    assert(layer_surface && full_area && remaining_area);

    //updates remaining area
    wlr_scene_layer_surface_v1_configure(layer_surface->scene_layer_surface_v1, full_area, remaining_area);
}

// Returns this layer surface's layer.
enum zwlr_layer_shell_v1_layer e_layer_surface_get_layer(struct e_layer_surface* layer_surface)
{
    assert(layer_surface);
    return layer_surface->scene_layer_surface_v1->layer_surface->current.layer;
}

// Returns this layer surface's wlr output.
struct wlr_output* e_layer_surface_get_wlr_output(struct e_layer_surface* layer_surface)
{
    assert(layer_surface);
    return layer_surface->scene_layer_surface_v1->layer_surface->output;
}
