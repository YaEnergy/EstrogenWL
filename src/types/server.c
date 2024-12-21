#include "types/server.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_seat.h>

#include "log.h"

#include "types/output.h"
#include "types/xdg_shell.h"
#include "types/input/input_manager.h"

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

    //allocate & configure output
    struct e_output* output = e_output_create(server, wlr_output);

    wl_list_insert(&server->outputs, &output->link);

    //output layout auto adds wl_output to the display, allows wl clients to find out information about the display
    //TODO: allow configuring the arrangement of outputs in the layout
    //TODO: check for possible memory allocation error?
    struct wlr_output_layout_output* layout_output = wlr_output_layout_add_auto(server->output_layout, wlr_output);
    struct wlr_scene_output* scene_output = wlr_scene_output_create(server->scene, wlr_output);
    wlr_scene_output_layout_add_output(server->scene_layout, layout_output, scene_output);
}

static void e_server_backend_destroy(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, backend_destroy);

    wl_list_remove(&server->outputs);
    wl_list_remove(&server->new_output.link);
    wl_list_remove(&server->backend_destroy.link);
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

    //init listener for new outputs
    wl_list_init(&server->outputs);
    server->new_output.notify = e_server_new_output;
    wl_signal_add(&server->backend->events.new_output, &server->new_output);

    server->backend_destroy.notify = e_server_backend_destroy;
    wl_signal_add(&server->backend->events.destroy, &server->backend_destroy);

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

    //allocates memory for pixel buffers 
    server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);

    if (server->allocator == NULL)
    {
        e_log_error("failed to create allocator");
        return 1;
    }

    //compositor, subcompositor and data device manager are wl globals

    //required for clients to allocate surfaces
    wlr_compositor_create(server->display, 5, server->renderer);
    //allows roles of subsurfaces to surfaces
    wlr_subcompositor_create(server->display);
    //handles clipboard
    wlr_data_device_manager_create(server->display);

    //wlroots utility for working with arrangement of screens in a physical layout
    server->output_layout = wlr_output_layout_create(server->display);

    //handles all rendering & damage tracking, 
    //use this to add renderable things to the scene graph 
    //and then call wlr_scene_commit_output to render the frame
    server->scene = wlr_scene_create();
    server->scene_layout = wlr_scene_attach_output_layout(server->scene, server->output_layout);
    
    //xdg shell v6, protocol for application windows
    server->xdg_shell = e_xdg_shell_create(server);

    //input device management
    server->input_manager = e_input_manager_create(server);

    return 0;
}

void e_server_destroy(struct e_server* server)
{
    e_input_manager_destroy(server->input_manager);

    wlr_scene_node_destroy(&server->scene->tree.node);
    wlr_allocator_destroy(server->allocator);
    wlr_renderer_destroy(server->renderer);
    wlr_backend_destroy(server->backend);
    wl_display_destroy(server->display);
}