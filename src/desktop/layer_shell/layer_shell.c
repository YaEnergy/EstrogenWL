#include "desktop/layer_shell.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>

#include "util/log.h"
#include "util/wl_macros.h"

#include "desktop/desktop.h"

#include "server.h"

// New wlr_layer_surface_v1.
static void layer_shell_new_layer_surface(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, new_layer_surface);
    struct wlr_layer_surface_v1* layer_surface = data;

    //output may be null
    if (layer_surface->output == NULL)
    {
        struct e_output* output = e_desktop_hovered_output(server->desktop);

        if (output == NULL)
        {
            e_log_error("layer_shell_new_layer_surface: layer surface output is NULL and not hovering over an output! Destroying layer surface");
            wlr_layer_surface_v1_destroy(layer_surface);
            return;
        }
        
        layer_surface->output = output->wlr_output;
    }
    
    if (e_layer_surface_create(server->desktop, layer_surface) == NULL)
    {
        e_log_error("layer_shell_new_layer_surface: failed to create layer surface");
        wlr_layer_surface_v1_destroy(layer_surface);
        return;
    }
}

// Create a layer shell.
// Returns NULL on fail.
bool e_server_init_layer_shell(struct e_server* server)
{
    assert(server);
    
    if (server == NULL)
        return false;

    server->layer_shell = wlr_layer_shell_v1_create(server->display, E_LAYER_SHELL_VERSION);

    if (server->layer_shell == NULL)
    {
        e_log_error("e_server_init_layer_shell: failed to create layer shell!");
        return false;
    }

    //events

    SIGNAL_CONNECT(server->layer_shell->events.new_surface, server->new_layer_surface, layer_shell_new_layer_surface);

    return true;
}

void e_server_fini_layer_shell(struct e_server* server)
{
    assert(server);

    if (server == NULL)
        return;

    SIGNAL_DISCONNECT(server->new_layer_surface);
}