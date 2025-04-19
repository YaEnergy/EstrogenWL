#include "server.h"

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
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_viewporter.h>

#include "desktop/desktop.h"
#include "desktop/gamma_control_manager.h"
#include "desktop/layer_shell.h"
#include "desktop/xdg_shell.h"
#if E_XWAYLAND_SUPPORT
#include "desktop/xwayland.h"
#endif

#include "util/log.h"
#include "util/wl_macros.h"

#include "input/seat.h"

#include "output.h"
#include "config.h"

static void e_server_new_input(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, new_input);
    struct wlr_input_device* input = data;

    if (server->desktop->seat != NULL)
        e_seat_add_input_device(server->desktop->seat, input);
}

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
    struct e_output* output = e_output_create(server->desktop, wlr_output);

    e_desktop_add_output(server->desktop, output);

#if E_XWAYLAND_SUPPORT
    if (server->xwayland != NULL)
        e_xwayland_update_workarea(server->xwayland);
#endif
}

static void e_server_backend_destroy(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, backend_destroy);

    SIGNAL_DISCONNECT(server->new_input);
    SIGNAL_DISCONNECT(server->new_output);
    SIGNAL_DISCONNECT(server->backend_destroy);
}

// GPU lost, destroy and recreate renderer
static void e_server_renderer_lost(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, renderer_lost);

    e_log_info("lost gpu! recreating renderer & allocator");

    // destroy old allocator and renderer

    wlr_allocator_destroy(server->allocator);

    SIGNAL_DISCONNECT(server->renderer_lost);

    wlr_renderer_destroy(server->renderer);

    // recreate & init new renderer
    
    server->renderer = wlr_renderer_autocreate(server->backend);

    if (server->renderer == NULL)
    {
        e_log_error("failed to create renderer");
        return;
    }

    if(!wlr_renderer_init_wl_display(server->renderer, server->display))
    {
        e_log_error("failed to init renderer & display");
        return;
    }

    SIGNAL_CONNECT(server->renderer->events.lost, server->renderer_lost, e_server_renderer_lost);

    // recreate allocator

    server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);

    if (server->allocator == NULL)
    {
        e_log_error("failed to create allocator");
        return;
    }

    // set compositor to use new renderer
    wlr_compositor_set_renderer(server->compositor, server->renderer);
}

int e_server_init(struct e_server* server)
{
    e_config_init(&server->config);

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

    //backend events
    SIGNAL_CONNECT(server->backend->events.new_input, server->new_input, e_server_new_input);
    SIGNAL_CONNECT(server->backend->events.new_output, server->new_output, e_server_new_output);
    SIGNAL_CONNECT(server->backend->events.destroy, server->backend_destroy, e_server_backend_destroy);

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
        e_log_error("failed to init renderer & display");
        return 1;
    }

    SIGNAL_CONNECT(server->renderer->events.lost, server->renderer_lost, e_server_renderer_lost);

    //allocates memory for pixel buffers 
    server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);

    if (server->allocator == NULL)
    {
        e_log_error("failed to create allocator");
        return 1;
    }

    //compositor, subcompositor and data device manager can handle everything we need them to do on their own

    //required for clients to allocate surfaces
    server->compositor = wlr_compositor_create(server->display, 5, server->renderer);
    //allows roles of subsurfaces to surfaces
    wlr_subcompositor_create(server->display);
    //handles clipboard
    wlr_data_device_manager_create(server->display);
    //allows clients to ask to copy part of the screen content to a client buffer, seems to be fully implemented by wlroots already
    wlr_screencopy_manager_v1_create(server->display);

    server->desktop = e_desktop_create(server->display, server->compositor, &server->config);

    if (server->desktop == NULL)
    {
        e_log_error("e_server_init: failed to create desktop");
        return 1;
    }

    //xdg shell v6, protocol for application views
    server->xdg_shell = e_xdg_shell_create(server->display, server->desktop);
    //protocol for layer surfaces
    server->layer_shell = e_layer_shell_create(server->display, server->desktop);

    #if E_XWAYLAND_SUPPORT
    //create & start xwayland server, xwayland shell protocol
    server->xwayland = e_xwayland_create(server->desktop, server->display, server->compositor, server->desktop->seat->wlr_seat, server->config.xwayland_lazy); 
    #endif

    //protocol to describe output regions, seems to be fully implemented by wlroots already
    wlr_xdg_output_manager_v1_create(server->display, server->desktop->output_layout);

    //enable viewporter => the size of the surface texture may not match the surface size anymore, only use surface size
    wlr_viewporter_create(server->display);

    //gamma control manager for output, does everything it needs to on its own
    //TODO: wlroots 0.19 will introduce wlr_scene_set_gamma_control_manager_v1, a helper that will remove the need for my implementation.
    e_gamma_control_manager_create(server->display);

    return 0;
}

bool e_server_run(struct e_server* server)
{
    // add unix socket to wl display
    const char* socket = wl_display_add_socket_auto(server->display);
    
    if (!socket)
    {
        e_log_error("failed to create socket");
        wlr_backend_destroy(server->backend);
        return false;
    }

    //set WAYLAND_DISPLAY env var
    setenv("WAYLAND_DISPLAY", socket, true);

#if E_XWAYLAND_SUPPORT
    //set DISPLAY env var for xwayland server
    if (server->xwayland != NULL)
    {
        e_log_info("xwayland DISPLAY=%s", server->xwayland->wlr_xwayland->display_name);
        setenv("DISPLAY", server->xwayland->wlr_xwayland->display_name, true);
    }
#endif

    //running
    e_log_info("starting backend");
    if (!wlr_backend_start(server->backend))
    {
        e_log_error("failed to start backend");
        wlr_backend_destroy(server->backend);
        wl_display_destroy(server->display);
        return false;
    }

    e_log_info("running wl display on WAYLAND_DISPLAY=%s", socket);
    wl_display_run(server->display);

    return true;
}

void e_server_fini(struct e_server* server)
{
    SIGNAL_DISCONNECT(server->renderer_lost);

#if E_XWAYLAND_SUPPORT
    e_xwayland_destroy(server->xwayland);
#endif

    wl_display_destroy_clients(server->display);

    e_desktop_destroy(server->desktop);

    wlr_allocator_destroy(server->allocator);
    wlr_renderer_destroy(server->renderer);
    wlr_backend_destroy(server->backend);
    wl_display_destroy(server->display);

    e_config_fini(&server->config);
}