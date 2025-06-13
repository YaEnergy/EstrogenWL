#include "desktop/output.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>

#include <time.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>

#include <wlr/util/box.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "desktop/tree/workspace.h"
#include "desktop/desktop.h"
#include "desktop/layer_shell.h"

#include "util/list.h"
#include "util/log.h"
#include "util/wl_macros.h"

#include "protocols/cosmic-workspace-v1.h"

#include "server.h"

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

static void output_fini_workspaces(struct e_output* output)
{
    assert(output);

    if (output == NULL)
        return;

    if (output->active_workspace != NULL)
        e_output_display_workspace(output, NULL);

    //destroy all workspaces
    for (int i = 0; i < output->workspace_group.workspaces.count; i++)
    {
        struct e_workspace* workspace = e_list_at(&output->workspace_group.workspaces, i);
        
        if (workspace != NULL)
            e_workspace_destroy(workspace);
    }

    e_list_fini(&output->workspace_group.workspaces);

    e_cosmic_workspace_group_output_leave(output->workspace_group.cosmic_handle, output->wlr_output);
    e_cosmic_workspace_group_remove(output->workspace_group.cosmic_handle);
}

static void e_output_handle_destroy(struct wl_listener* listener, void* data)
{
    struct e_output* output = wl_container_of(listener, output, destroy);

    output_fini_workspaces(output);

    wlr_scene_node_destroy(&output->tree->node);
    output->tree = NULL;
    
    if (output->scene_output != NULL)
    {
        wlr_scene_output_destroy(output->scene_output);
        output->scene_output = NULL;
    }

    if (output->layout != NULL)
    {
        wlr_output_layout_remove(output->layout, output->wlr_output);
        output->layout = NULL;
    }

    SIGNAL_DISCONNECT(output->frame);
    SIGNAL_DISCONNECT(output->request_state);
    SIGNAL_DISCONNECT(output->destroy);

    wl_list_remove(&output->link);

    output->wlr_output->data = NULL;
    
    free(output);
}

struct e_output* e_output_create(struct wlr_output* wlr_output)
{
    assert(wlr_output);

    if (wlr_output == NULL)
        return NULL;

    struct e_output* output = calloc(1, sizeof(*output));

    if (output == NULL)
    {
        e_log_error("e_output_create: failed to alloc e_output");
        return NULL;
    }

    output->server = NULL;
    output->wlr_output = wlr_output;
    output->layout = NULL;
    output->scene_output = NULL;

    output->active_workspace = NULL;
    output->usable_area = (struct wlr_box){0, 0, 0, 0};

    wl_list_init(&output->link);
    wl_list_init(&output->layer_surfaces);

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

    if (output == NULL || output->layout == NULL)
        return;

    struct wlr_box full_area = (struct wlr_box){0, 0, 0, 0};
    wlr_output_layout_get_box(output->layout, output->wlr_output, &full_area);

    //FIXME: remaining area can become too small, I'm not sure what to do in those edge cases yet
    struct wlr_box remaining_area = full_area;

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

static void output_init_mode(struct wlr_output* output)
{
    assert(output);

    if (output == NULL)
        return;

    //enable state if neccessary
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    //set mode if backend is DRM+KMS, otherwise can't use the output
    //automatically pick preferred mode for now
    struct wlr_output_mode* mode = wlr_output_preferred_mode(output);
    if (!wl_list_empty(&output->modes))
        wlr_output_state_set_mode(&state, mode);

    //apply new output state
    wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);    
}

static void output_init_scene(struct e_server* server, struct e_output* output)
{
    assert(output && server);

    if (output == NULL || server == NULL)
        return;

    output->tree = wlr_scene_tree_create(&server->desktop->scene->tree);
    wlr_scene_node_lower_to_bottom(&output->tree->node);
    
    //create scene trees for all layers

    output->layers.background = wlr_scene_tree_create(output->tree);
    output->layers.bottom = wlr_scene_tree_create(output->tree);
    output->layers.tiling = wlr_scene_tree_create(output->tree);
    output->layers.floating = wlr_scene_tree_create(output->tree);
    output->layers.top = wlr_scene_tree_create(output->tree);
    output->layers.overlay = wlr_scene_tree_create(output->tree);
}

static bool output_init_layout(struct e_desktop* desktop, struct e_output* output)
{
    assert(desktop && output);

    if (desktop == NULL || output == NULL)
        return false;

    struct wlr_output* wlr_output = output->wlr_output;

    //output layout auto adds wl_output to the display, allows wl clients to find out information about the display
    //TODO: allow configuring the arrangement of outputs in the layout
    struct wlr_output_layout_output* layout_output = wlr_output_layout_add_auto(desktop->output_layout, wlr_output);

    if (layout_output == NULL)
    {
        e_log_error("e_output_init_layout: failed to add output to layout");
        return false;
    }

    struct wlr_scene_output* scene_output = wlr_scene_output_create(desktop->scene, wlr_output);

    if (scene_output == NULL)
    {
        e_log_error("e_output_init_layout: failed to create scene output");
        wlr_output_layout_remove(desktop->output_layout, wlr_output);
        return false;
    }

    wlr_scene_output_layout_add_output(desktop->scene_layout, layout_output, scene_output);

    output->layout = desktop->output_layout;
    output->scene_output = scene_output;

    wl_list_insert(&desktop->outputs, &output->link);

    return true;
}

static bool output_init_workspaces(struct e_output* output)
{
    assert(output && output->server && output->layout);

    if (output == NULL || output->server == NULL || output->layout == NULL)
        return false;

    output->workspace_group.cosmic_handle = e_cosmic_workspace_group_create(output->server->cosmic_workspace_manager);
    e_cosmic_workspace_group_output_enter(output->workspace_group.cosmic_handle, output->wlr_output);

    //create 5 workspaces for output
    e_list_init(&output->workspace_group.workspaces, 5);

    for (int i = 0; i < 5; i++)
    {
        struct e_workspace* workspace = e_workspace_create(output);

        //give number names
        char name[16];
        snprintf(name, sizeof(name), "%i", i + 1);
        e_workspace_set_name(workspace, name);

        if (workspace != NULL)
            e_list_add(&output->workspace_group.workspaces, workspace);
        else
            e_log_error("e_output_init_workspaces: failed to create workspace %i", i + 1);
    }

    e_output_display_workspace(output, e_list_at(&output->workspace_group.workspaces, 0));

    e_output_arrange(output);

    return true;
}

static void server_new_output(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, new_output);
    struct wlr_output* wlr_output = data;

    //non-desktop outputs such as VR are unsupported
    if (wlr_output->non_desktop)
    {
        e_log_info("e_server_new_output: new unsupported non-desktop output, skipping...");
        return;
    }

    //configure output by backend to use allocator and renderer
    //before committing output
    if (!wlr_output_init_render(wlr_output, server->allocator, server->renderer))
    {
        e_log_error("e_server_new_output: failed to init output's rendering subsystem");
        return;
    }

    output_init_mode(wlr_output);

    //allocate & configure output
    struct e_output* output = e_output_create(wlr_output);

    if (output == NULL)
    {
        e_log_error("server_new_output: failed to create desktop output!");
        return;
    }

    output->server = server;
    
    if (!output_init_layout(server->desktop, output))
    {
        e_log_error("server_new_output: failed to init layout!");
        e_output_destroy(output);
        return;
    }

    output_init_scene(server, output);

    if (!output_init_workspaces(output))
    {
        e_log_info("server_new_output: failed to init workspaces");
        e_output_destroy(output);
        return;
    }

    //TODO: update xwayland workarea
}

bool e_server_init_outputs(struct e_server* server)
{
    assert(server);

    if (server == NULL)
        return false;

    SIGNAL_CONNECT(server->backend->events.new_output, server->new_output, server_new_output);

    //TODO: move all output specific protocols to here aswell (mostly copy part of output stuff)

    //allows clients to ask to copy part of the screen content to a client buffer, seems to be fully implemented by wlroots already
    if (wlr_screencopy_manager_v1_create(server->display) == NULL)
        e_log_error("e_server_init_outputs: failed to create wlr screencopy manager");

    //low overhead screen content capturing
    if (wlr_export_dmabuf_manager_v1_create(server->display) == NULL)
        e_log_error("e_server_init_outputs: failed to create wlr export dmabuf manager v1");
    
    //more screen capturing
    if (wlr_ext_output_image_capture_source_manager_v1_create(server->display, E_EXT_IMAGE_CAPTURE_SOURCE_VERSION) == NULL)
        e_log_error("e_server_init_outputs: failed to create wlr ext output image capture source manager v1");

    //output toplevel capturing
    if (wlr_ext_image_copy_capture_manager_v1_create(server->display, E_EXT_IMAGE_COPY_CAPTURE_VERSION) == NULL)
        e_log_error("e_server_init_outputs: failed to create wlr ext image copy capture manager v1");

    return true;
}

void e_server_fini_outputs(struct e_server* server)
{
    assert(server);

    if (server == NULL)
        return;

    SIGNAL_DISCONNECT(server->new_output);
}