#include "types/output.h"
#include "types/server.h"

#include <bits/time.h>
#include <stdlib.h>

#include <time.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/backend.h>

void e_output_frame(struct wl_listener* listener, void* data)
{
    struct e_output* output = wl_container_of(listener, output, frame);

    // TODO: get scene, and it's output and render it by committing
    struct wlr_scene* scene = output->server->scene;
    
    struct wlr_scene_output* scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);

    //render scene and commit output
    wlr_scene_output_commit(scene_output, NULL);

    //send frame from this timestamp
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

void e_output_destroy(struct wl_listener* listener, void* data)
{
    struct e_output* output = wl_container_of(listener, output, destroy);

    wl_list_remove(&output->link);
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->destroy.link);
    free(output);
}
