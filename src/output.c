#include "output.h"

#include <bits/time.h>
#include <stdlib.h>

#include <time.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/backend.h>

#include "desktop/layer_shell.h"
#include "desktop/scene.h"

#include "server.h"

#include "util/log.h"
#include "wm.h"

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

    if (!wlr_output_commit_state(output->wlr_output, event->state))
        e_log_error("Failed to commit output request state");
    
    //update layers & windows to arrange & retile them according to the new output size
    e_tile_windows(output->server);
    e_layer_shell_arrange_all_layers(output->server->layer_shell, output->wlr_output);
}

static void e_output_destroy(struct wl_listener* listener, void* data)
{
    struct e_output* output = wl_container_of(listener, output, destroy);

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

    //add signals
    wl_list_init(&output->link);

    output->frame.notify = e_output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);

    output->request_state.notify = e_output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);

    output->destroy.notify = e_output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    return output;
}