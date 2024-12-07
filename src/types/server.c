#include "types/server.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>

#include "log.h"

#include "types/output.h"

static void e_server_new_output(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, new_output);
    struct wlr_output* wlr_output = data;

    //configure output by backend to use allocator and renderer
    //before committing output
    wlr_output_init_render(wlr_output, server->allocator, server->renderer);

    //enable state if neccessary
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    //set mode if backend is DRM+KMS, otherwise can't use the output
    //automatically pick preferred mode for now
    struct wlr_output_mode* mode = wlr_output_preferred_mode(wlr_output);
    if (!wl_list_empty(&wlr_output->modes))
        wlr_output_state_set_mode(&state, mode);

    //apply new output state
    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);

    struct e_output* output = calloc(1, sizeof(*output));

    //TODO: frame listener for when to draw frames?

    //TODO: request state listener for requesting state?

    output->destroy.notify = e_output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    wl_list_insert(&server->outputs, &output->link);
}

int e_server_init(struct e_server *server)
{
    //handles accepting clients from Unix socket, managing wl globals, ...
    e_log_info("creating display...");
    server->display = wl_display_create();
     if (server->display == NULL)
    {
        e_log_error("failed to create display");
        return 1;
    }

    //backend handles input and output hardware, autocreate automatically creates the backend we want
    e_log_info("creating backend...");
    server->backend = wlr_backend_autocreate(wl_display_get_event_loop(server->display), NULL);
    if (server->backend == NULL)
    {
        e_log_error("failed to create backend");
        return 1;
    }

    //TODO: output notify
    wl_list_init(&server->outputs);

    server->new_output.notify = e_server_new_output;
    wl_signal_add(&server->backend->events.new_output, &server->new_output);

    //renderer handles rendering
    e_log_info("creating renderer...");
    server->renderer = wlr_renderer_autocreate(server->backend);
    if (server->renderer == NULL)
    {
        e_log_error("failed to create renderer");
        return 1;
    }

    if(!wlr_renderer_init_wl_display(server->renderer, server->display))
    {
        e_log_error("failed to init buffer factory protocols");
        return 1;
    }

    server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
    if (server->allocator == NULL)
    {
        e_log_error("failed to create allocator");
        return 1;
    }

    return 0;
}