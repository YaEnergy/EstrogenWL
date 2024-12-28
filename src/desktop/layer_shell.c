#include "desktop/layer_shell.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_layer_shell_v1.h>

#include "desktop/layer_surface.h"
#include "util/log.h"

#include "desktop/scene.h"
#include "server.h"

//new wlr_layer_surface_v1 that has been configured by client to configure and commit
static void e_layer_shell_new_surface(struct wl_listener* listener, void* data)
{
    struct e_layer_shell* layer_shell = wl_container_of(listener, layer_shell, new_surface);
    struct wlr_layer_surface_v1* layer_surface = data;

    //output may be null
    if (layer_surface->output == NULL)
    {
        struct e_output* output = e_scene_get_output(layer_shell->server->scene, 0);

        if (output == NULL)
        {
            e_log_error("Server has no output!");
            abort();
        }
        
        layer_surface->output = output->wlr_output;
    }
    
    e_layer_surface_create(layer_shell->server, layer_surface);

    //honour layer_surface's desired size or dimensions I guess, I don't really have a reason to reject them
    //wlr_layer_surface_v1_configure(layer_surface, layer_surface->current.desired_width, layer_surface->current.desired_height);
}

static void e_layer_shell_destroy(struct wl_listener* listener, void* data)
{
    struct e_layer_shell* layer_shell = wl_container_of(listener, layer_shell, destroy);

    wl_list_remove(&layer_shell->new_surface.link);
    wl_list_remove(&layer_shell->destroy.link);

    free(layer_shell);
}

struct e_layer_shell* e_layer_shell_create(struct e_server* server)
{
    struct e_layer_shell* layer_shell = calloc(1, sizeof(struct e_layer_shell));
    layer_shell->server = server;

    layer_shell->wlr_layer_shell_v1 = wlr_layer_shell_v1_create(server->display, 4);

    //events

    layer_shell->new_surface.notify = e_layer_shell_new_surface;
    wl_signal_add(&layer_shell->wlr_layer_shell_v1->events.new_surface, &layer_shell->new_surface);

    layer_shell->destroy.notify = e_layer_shell_destroy;
    wl_signal_add(&layer_shell->wlr_layer_shell_v1->events.destroy, &layer_shell->destroy);

    return layer_shell;
}