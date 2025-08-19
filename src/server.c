#include "server.h"

#include <signal.h>
#include <stdlib.h>
#include <assert.h>

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
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>

#if E_XWAYLAND_SUPPORT
#include <wlr/xwayland.h>
#endif

#include "desktop/desktop.h"
#include "desktop/output.h"

#include "util/log.h"
#include "util/wl_macros.h"

#include "input/seat.h"

#include "protocols/cosmic-workspace-v1.h"
#include "protocols/ext-workspace-v1.h"

#include "config.h"

static bool e_server_init_scene(struct e_server* server)
{
    assert(server && server->display && server->output_layout);

    if (server == NULL)
        return false;

    server->scene = wlr_scene_create();
    server->scene_layout = wlr_scene_attach_output_layout(server->scene, server->output_layout);

    server->unmanaged = wlr_scene_tree_create(&server->scene->tree);

    server->pending = wlr_scene_tree_create(&server->scene->tree);
    wlr_scene_node_set_enabled(&server->pending->node, false);

    wl_list_init(&server->view_containers);

    //gamma control manager for outputs
    struct wlr_gamma_control_manager_v1* gamma_control_manager = wlr_gamma_control_manager_v1_create(server->display);

    if (gamma_control_manager != NULL)
        wlr_scene_set_gamma_control_manager_v1(server->scene, gamma_control_manager);
    else
        e_log_error("e_server_init_scene: failed to create wlr gamma control manager v1");

    return true;
}

static void e_server_fini_scene(struct e_server* server)
{
    assert(server);

    if (server == NULL)
        return;

    //destroy root node
    wlr_scene_node_destroy(&server->scene->tree.node);
}

// Handle signals that we use to terminate the server.
static int e_server_handle_signal_terminate(int signal, void* data)
{
    struct e_server* server = data;
    e_server_terminate(server);
    return 0;
}

static void e_server_new_input(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, new_input);
    struct wlr_input_device* input = data;

    if (server->seat != NULL)
        e_seat_add_input_device(server->seat, input);
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

    //reinit outputs
    struct e_output* output;
    wl_list_for_each(output, &server->outputs, link)
    {
        if (!wlr_output_init_render(output->wlr_output, server->allocator, server->renderer))
            e_log_error("e_server_renderer_lost: failed to init an output's rendering subsystem");
    }
}

int e_server_init(struct e_server* server, struct e_config* config)
{
    assert(server && config);

    if (server == NULL || config == NULL)
        return 1;

    server->config = config;

    //handles accepting clients from Unix socket, managing wl globals, ...
    e_log_info("creating display...");
    server->display = wl_display_create();

    if (server->display == NULL)
    {
        e_log_error("failed to create display");
        return 1;
    }

    server->event_loop = wl_display_get_event_loop(server->display);

    //handle event source signals
    server->sources.sigint = wl_event_loop_add_signal(server->event_loop, SIGINT, e_server_handle_signal_terminate, server);
    server->sources.sigterm = wl_event_loop_add_signal(server->event_loop, SIGTERM, e_server_handle_signal_terminate, server);
    //TODO: sighup & sigchld?

    //according to wayfire (who discovered this), for this to work inside of gtk apps this must be one of the first globals
    //I read this inside labwc
    //Allows a shortcut for pasting text, usually middle click
    struct wlr_primary_selection_v1_device_manager* primary_selection_v1_device_manager = wlr_primary_selection_v1_device_manager_create(server->display);

    if (primary_selection_v1_device_manager == NULL)
        e_log_error("e_server_init: failed to create wlr_primary_selection_v1_device_manager");

    //backend handles input and output hardware, autocreate automatically creates the backend we want
    e_log_info("creating backend...");
    server->backend = wlr_backend_autocreate(server->event_loop, NULL);

    if (server->backend == NULL)
    {
        e_log_error("failed to create backend");
        return 1;
    }

    //backend events
    SIGNAL_CONNECT(server->backend->events.new_input, server->new_input, e_server_new_input);

    //renderer handles rendering
    e_log_info("creating renderer...");
    server->renderer = wlr_renderer_autocreate(server->backend);

    if (server->renderer == NULL)
    {
        e_log_error("failed to create renderer");
        return 1;
    }

    wlr_renderer_init_wl_shm(server->renderer, server->display);
    
    SIGNAL_CONNECT(server->renderer->events.lost, server->renderer_lost, e_server_renderer_lost);

    struct wlr_linux_dmabuf_v1* linux_dmabuf = NULL;

    if (wlr_renderer_get_texture_formats(server->renderer, WLR_BUFFER_CAP_DMABUF) != NULL)
    {
        linux_dmabuf = wlr_linux_dmabuf_v1_create_with_renderer(server->display, E_LINUX_DMABUF_VERSION, server->renderer);

        if (linux_dmabuf == NULL)
        {
            e_log_error("e_server_init: dmabuf buffer capability, but failed to create wlr linux dmabuf v1 with renderer");
            return 1;
        }
    }

    int drm_fd = wlr_renderer_get_drm_fd(server->renderer);

    e_log_info("drm fd: %i, renderer timeline support: %i, backend timeline support: %i", drm_fd, server->renderer->features.timeline, server->backend->features.timeline);

    if (drm_fd >= 0 && server->renderer->features.timeline && server->backend->features.timeline)
    {
        e_log_info("Server has support for explicit synchronization! Enabling...");

        if (wlr_linux_drm_syncobj_manager_v1_create(server->display, E_LINUX_DRM_SYNCOBJ_VERSION, drm_fd) == NULL)
            e_log_error("e_server_init: server has support for explicit synchronization, but failed to create wlr_linux_drm_syncobj_manager_v1");
    }

    //allocates memory for pixel buffers 
    server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);

    if (server->allocator == NULL)
    {
        e_log_error("failed to create allocator");
        return 1;
    }

    //compositor, subcompositor and data device manager can handle everything we need them to do on their own

    //required for clients to allocate surfaces
    server->compositor = wlr_compositor_create(server->display, E_COMPOSITOR_VERSION, server->renderer);
    //allows roles of subsurfaces to surfaces
    wlr_subcompositor_create(server->display);
    //handles clipboard
    wlr_data_device_manager_create(server->display);
    
    //TODO: log more errors here

    //allows clients to control selection and take the role of a clipboard manager
    wlr_data_control_manager_v1_create(server->display);
    wlr_ext_data_control_manager_v1_create(server->display, E_EXT_DATA_CONTROL_V1_VERSION);

    if (!e_server_init_outputs(server))
    {
        e_log_error("e_server_init: failed to init outputs");
        return 1;
    }

    if (!e_server_init_scene(server))
    {
        e_log_error("e_server_init: failed to init scene");
        return 1;
    }

    if (linux_dmabuf != NULL)
        wlr_scene_set_linux_dmabuf_v1(server->scene, linux_dmabuf);

    //input device management
    server->seat = e_seat_create(server, server->output_layout, "seat0");

    if (server->seat == NULL)
    {
        e_log_error("e_server_init: failed to create seat0");
        return 1;
    }

    //xdg shell v6, protocol for application views
    if (!e_server_init_xdg_shell(server))
    {
        e_log_error("e_server_init: failed to init xdg shell");
        return 1;
    }

    //protocol for layer surfaces
    if (!e_server_init_layer_shell(server))
    {
        e_log_error("e_server_init: failed to init layer shell");
        return 1;
    }

    #if E_XWAYLAND_SUPPORT
    //create & start xwayland server, xwayland shell protocol
    if (!e_server_init_xwayland(server, server->seat, server->config->xwayland_lazy))
    {
        e_log_error("e_server_init: failed to init xwayland");
        return 1;
    }
    #endif

    //protocol to describe output regions
    if (wlr_xdg_output_manager_v1_create(server->display, server->output_layout) == NULL)
        e_log_error("e_server_init: failed to create wlr_xdg_output_manager_v1");

    //enable viewporter => the size of the surface texture may not match the surface size anymore, only use surface size
    struct wlr_viewporter* viewporter = wlr_viewporter_create(server->display);

    if (viewporter == NULL)
        e_log_error("e_server_init: failed to create wlr_viewporter");
    
    if (wlr_presentation_create(server->display, server->backend, E_PRESENTATION_TIME_VERSION) == NULL)
        e_log_error("e_server_init: failed to create wlr_presentation");

    if (wlr_single_pixel_buffer_manager_v1_create(server->display) == NULL)
        e_log_error("e_server_init: failed to create wlr single pixel buffer manager v1");

    //allow clients to set a factor for the alpha values on a surface
    if (wlr_alpha_modifier_v1_create(server->display) == NULL)
        e_log_error("e_server_init: failed to create wlr alpha modifier v1");

    server->foreign_toplevel_list = wlr_ext_foreign_toplevel_list_v1_create(server->display, E_EXT_FOREIGN_TOPLEVEL_LIST_VERSION);

    if (server->foreign_toplevel_list == NULL)
    {
        e_log_error("e_server_init: failed to create foreign toplevel list");
        return 1;
    }

    server->foreign_toplevel_manager = wlr_foreign_toplevel_manager_v1_create(server->display);

    if (server->foreign_toplevel_manager == NULL)
    {
        e_log_error("e_server_init: failed to create foreign toplevel manager");
        return 1;
    }

    server->cosmic_workspace_manager = e_cosmic_workspace_manager_create(server->display, E_COSMIC_WORKSPACE_VERSION, E_COSMIC_WORKSPACE_CAPABILITY_ACTIVATE);

    if (server->cosmic_workspace_manager == NULL)
    {
        e_log_error("e_server_init: failed to create cosmic workspace manager");
        return 1;
    }

    server->ext_workspace_manager = e_ext_workspace_manager_create(server->display, E_EXT_WORKSPACE_VERSION, E_EXT_WORKSPACE_CAPABILITY_ACTIVATE);

    if (server->ext_workspace_manager == NULL)
    {
        e_log_error("e_server_init: failed to create ext workspace manager");
        return 1;
    }

    //allows clients to reference surfaces of other clients
    struct wlr_xdg_foreign_registry* foreign_registry = wlr_xdg_foreign_registry_create(server->display);
    wlr_xdg_foreign_v1_create(server->display, foreign_registry);
    wlr_xdg_foreign_v2_create(server->display, foreign_registry);

    e_desktop_state_init(&server->desktop_state);

    return 0;
}

bool e_server_start(struct e_server* server)
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
        e_log_info("xwayland DISPLAY=%s", server->xwayland->display_name);
        setenv("DISPLAY", server->xwayland->display_name, true);
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

    e_log_info("WAYLAND_DISPLAY=%s", socket);

    return true;
}

void e_server_run(struct e_server* server)
{
    assert(server);

    if (server == NULL)
        return;

    e_log_info("running wl display...");
    wl_display_run(server->display);
}

void e_server_terminate(struct e_server* server)
{
    assert(server);

    if (server == NULL)
        return;

    e_log_info("terminating server's wl display");
    wl_display_terminate(server->display);
}

void e_server_fini(struct e_server* server)
{
    assert(server);

    if (server == NULL)
        return;

    SIGNAL_DISCONNECT(server->new_input);
    SIGNAL_DISCONNECT(server->renderer_lost);

    //remove event source signals
    wl_event_source_remove(server->sources.sigint);
    wl_event_source_remove(server->sources.sigterm);

#if E_XWAYLAND_SUPPORT
    e_server_fini_xwayland(server);
#endif

    wl_display_destroy_clients(server->display);

    e_seat_destroy(server->seat);
    
    e_server_fini_xdg_shell(server);
    e_server_fini_layer_shell(server);

    e_server_fini_outputs(server);

    e_server_fini_scene(server);

    wlr_allocator_destroy(server->allocator);
    wlr_renderer_destroy(server->renderer);
    wlr_backend_destroy(server->backend);
    wl_display_destroy(server->display);
}
