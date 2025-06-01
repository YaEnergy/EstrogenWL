#include "desktop/output.h"

#include <time.h>
#include <stdlib.h>
#include <assert.h>

#include <time.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/backend.h>

#include <wlr/util/box.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "desktop/tree/workspace.h"
#include "desktop/desktop.h"
#include "desktop/layer_shell.h"

#include "util/log.h"
#include "util/wl_macros.h"

static void e_output_frame(struct wl_listener* listener, void* data)
{
    struct e_output* output = wl_container_of(listener, output, frame);

    if (output->scene_output == NULL)
        return;

    //render scene output viewport, commit its output to show it, and send frame from this timestamp
    wlr_scene_output_commit(output->scene_output, NULL);

    //send frame from this timestamp
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(output->scene_output, &now);
}

static void e_output_request_state(struct wl_listener* listener, void* data)
{
    //just commit new state, so the x11 or wayland backend window resizes properly

    struct e_output* output = wl_container_of(listener, output, request_state);
    struct wlr_output_event_request_state* event = data;

    //rearrange output accordingly to the new output buffer on commit success
    if (wlr_output_commit_state(output->wlr_output, event->state))
        e_output_arrange(output);
    else 
        e_log_error("Failed to commit output request state");
}

static void e_output_handle_destroy(struct wl_listener* listener, void* data)
{
    struct e_output* output = wl_container_of(listener, output, destroy);

    if (output->active_workspace != NULL)
        e_workspace_set_activated(output->active_workspace, false);
    
    wl_list_init(&output->link);
    wl_list_remove(&output->link);

    SIGNAL_DISCONNECT(output->frame);
    SIGNAL_DISCONNECT(output->request_state);
    SIGNAL_DISCONNECT(output->destroy);
    
    free(output);
}

struct e_output* e_output_create(struct e_desktop* desktop, struct wlr_scene_output* scene_output)
{
    struct e_output* output = calloc(1, sizeof(*output));

    if (output == NULL)
    {
        e_log_error("e_output_create: failed to alloc e_output");
        return NULL;
    }

    struct wlr_output* wlr_output = scene_output->output;

    output->desktop = desktop;
    output->wlr_output = wlr_output;
    output->layout = desktop->output_layout;
    output->scene_output = scene_output;

    output->active_workspace = NULL;
    output->usable_area = (struct wlr_box){0, 0, 0, 0};

    wl_list_init(&output->layer_surfaces);

    wl_list_insert(&desktop->outputs, &output->link);

    wlr_output->data = output;

    // events

    SIGNAL_CONNECT(wlr_output->events.frame, output->frame, e_output_frame);
    SIGNAL_CONNECT(wlr_output->events.request_state, output->request_state, e_output_request_state);
    SIGNAL_CONNECT(wlr_output->events.destroy, output->destroy, e_output_handle_destroy);

    return output;
}

static void e_output_arrange_layer(struct e_output* output, enum zwlr_layer_shell_v1_layer layer, struct wlr_box* full_area, struct wlr_box* remaining_area)
{
    assert(output && full_area && remaining_area);

    if (wl_list_empty(&output->layer_surfaces))
        return;

    //configure each layer surface in this output & layer
    struct e_layer_surface* layer_surface;
    wl_list_for_each(layer_surface, &output->layer_surfaces, link)
    {
        //is this surface on this output & layer? if so, then configure and update remaining area
        if (e_layer_surface_get_layer(layer_surface) == layer)
            e_layer_surface_configure(layer_surface, full_area, remaining_area);
    }
}

static void e_output_arrange_all_layers(struct e_output* output, struct wlr_box* full_area, struct wlr_box* remaining_area)
{
    assert(output && full_area && remaining_area);

    e_output_arrange_layer(output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, full_area, remaining_area);
    e_output_arrange_layer(output, ZWLR_LAYER_SHELL_V1_LAYER_TOP, full_area, remaining_area);
    e_output_arrange_layer(output, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, full_area, remaining_area);
    e_output_arrange_layer(output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, full_area, remaining_area);
}

// Get topmost layer surface that requests exclusive focus.
// Returns NULL if none.
struct e_layer_surface* e_output_get_exclusive_topmost_layer_surface(struct e_output* output)
{
    if (wl_list_empty(&output->layer_surfaces))
        return NULL;

    struct e_layer_surface* topmost_layer_surface = NULL;
    struct e_layer_surface* layer_surface;

    wl_list_for_each(layer_surface, &output->layer_surfaces, link)
    {
        struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->scene_layer_surface_v1->layer_surface;

        //must request exclusive focus
        if (wlr_layer_surface_v1->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
            continue;

        //highest layer lives
        if (topmost_layer_surface == NULL || wlr_layer_surface_v1->current.layer > e_layer_surface_get_layer(topmost_layer_surface))
            topmost_layer_surface = layer_surface;
    }

    return topmost_layer_surface;
}

// Display given workspace.
// Given workspace must be inactive, but is allowed to be NULL.
bool e_output_display_workspace(struct e_output* output, struct e_workspace* workspace)
{
    if (output == NULL)
    {
        e_log_error("e_output_set_workspace: no output given!");
        return false;
    }

    //must be inactive if workspace is given
    if (workspace != NULL && workspace->active)
    {
        e_log_error("e_output_set_workspace: workspace must be inactive!");
        return false;
    }

    //deactivate previous workspace
    if (output->active_workspace != NULL)
        e_workspace_set_activated(output->active_workspace, false);

    //activate new workspace

    output->active_workspace = workspace;

    if (workspace != NULL)
    {
        e_workspace_set_activated(workspace, true);

        struct wlr_box full_area = (struct wlr_box){0, 0, 0, 0};
        wlr_output_layout_get_box(output->layout, output->wlr_output, &full_area);
        e_workspace_arrange(workspace, full_area, output->usable_area);
    }

    return true;
}

void e_output_arrange(struct e_output* output)
{
    assert(output && output->layout);

    struct wlr_box full_area = (struct wlr_box){0, 0, 0, 0};
    wlr_output_layout_get_box(output->layout, output->wlr_output, &full_area);

    //FIXME: remaining area can become too small, I'm not sure what to do in those edge cases yet
    struct wlr_box remaining_area = full_area;

    if (output->desktop != NULL)
        e_output_arrange_all_layers(output, &full_area, &remaining_area);

    if (output->active_workspace != NULL)
        e_workspace_arrange(output->active_workspace, full_area, remaining_area);
    
    #if E_VERBOSE
    e_log_info("full area: (%i, %i) %ix%i", full_area.x, full_area.y, full_area.width, full_area.height);
    e_log_info("remaining area: (%i, %i) %ix%i", remaining_area.x, remaining_area.y, remaining_area.width, remaining_area.height);
    #endif

    output->usable_area = remaining_area;
}

// Destroy the output.
void e_output_destroy(struct e_output* output)
{
    assert(output);

    if (output != NULL)
        wlr_output_destroy(output->wlr_output);
}