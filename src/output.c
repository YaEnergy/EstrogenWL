#include "output.h"

#include <bits/time.h>
#include <stdlib.h>
#include <assert.h>

#include <time.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/backend.h>

#include "desktop/layers/layer_shell.h"
#include "desktop/scene.h"

#include "desktop/tree/container.h"
#include "server.h"

#include "util/log.h"

static void e_output_frame(struct wl_listener* listener, void* data)
{
    struct e_output* output = wl_container_of(listener, output, frame);
    struct wlr_scene* scene = output->server->scene->wlr_scene;
    
    //render scene, commit its output to show it, and send frame from this timestamp

    struct wlr_scene_output* scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);

    //render scene and commit output
    wlr_scene_output_commit(scene_output, NULL);

    //send frame from this timestamp
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
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

static void e_output_destroy(struct wl_listener* listener, void* data)
{
    struct e_output* output = wl_container_of(listener, output, destroy);

    if (output->root_tiling_container != NULL)
    {
        e_container_destroy(output->root_tiling_container);
        output->root_tiling_container = NULL;
    }

    if (output->root_floating_container != NULL)
    {
        e_container_destroy(output->root_floating_container);
        output->root_floating_container = NULL;
    }

    wl_list_init(&output->link);
    wl_list_remove(&output->link);
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);
    wl_list_remove(&output->destroy.link);
    
    free(output);
}

struct e_output* e_output_create(struct e_server* server, struct wlr_output* wlr_output)
{
    struct e_output* output = calloc(1, sizeof(*output));
    output->wlr_output = wlr_output;
    output->server = server;
    output->root_tiling_container = NULL;
    output->root_floating_container = NULL;

    //add signals
    wl_list_init(&output->link);

    output->frame.notify = e_output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);

    output->request_state.notify = e_output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);

    output->destroy.notify = e_output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    wlr_output->data = output;

    return output;
}

void e_output_arrange(struct e_output* output)
{
    assert(output && output->root_tiling_container && output->root_floating_container && output->layout);

    struct wlr_box useable_area = (struct wlr_box){0, 0, 0, 0};
    wlr_output_layout_get_box(output->layout, output->wlr_output, &useable_area);

    //TODO: account for output's useable area after layer shell arrangement
    if (output->server->layer_shell != NULL)
        e_layer_shell_arrange_all_layers(output->server->layer_shell, output->wlr_output);

    if (output->root_tiling_container != NULL)
    {
        e_container_configure(output->root_tiling_container, useable_area.x, useable_area.y, useable_area.width, useable_area.height);
        e_container_arrange(output->root_tiling_container);
    }

    if (output->root_floating_container != NULL)
    {
        e_container_configure(output->root_floating_container, useable_area.x, useable_area.y, useable_area.width, useable_area.height);
        e_container_arrange(output->root_floating_container);
    }   
}