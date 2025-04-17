#include "desktop/layers/layer_shell.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>

#include "desktop/layers/layer_surface.h"
#include "util/log.h"

#include "desktop/desktop.h"

//new wlr_layer_surface_v1 that has been configured by client to configure and commit
static void e_layer_shell_new_surface(struct wl_listener* listener, void* data)
{
    struct e_layer_shell* layer_shell = wl_container_of(listener, layer_shell, new_surface);
    struct wlr_layer_surface_v1* layer_surface = data;

    //TODO: if output is NULL, set to the latest focused output

    //output may be null
    if (layer_surface->output == NULL)
    {
        struct e_output* output = e_desktop_get_output(layer_shell->desktop, 0);

        if (output == NULL)
        {
            e_log_error("Desktop has no output!");
            abort();
        }
        
        layer_surface->output = output->wlr_output;
    }
    
    e_layer_surface_create(layer_shell->desktop, layer_surface);
}

static void e_layer_shell_destroy(struct wl_listener* listener, void* data)
{
    struct e_layer_shell* layer_shell = wl_container_of(listener, layer_shell, destroy);

    wl_list_remove(&layer_shell->new_surface.link);
    wl_list_remove(&layer_shell->destroy.link);

    free(layer_shell);
}

struct e_layer_shell* e_layer_shell_create(struct wl_display* display, struct e_desktop* desktop)
{
    struct e_layer_shell* layer_shell = calloc(1, sizeof(*layer_shell));

    if (layer_shell == NULL)
    {
        e_log_error("e_layer_shell_create: failed to alloc e_layer_shell");
        return NULL;
    }

    layer_shell->desktop = desktop;

    layer_shell->wlr_layer_shell_v1 = wlr_layer_shell_v1_create(display, E_LAYER_SHELL_VERSION);

    //events

    layer_shell->new_surface.notify = e_layer_shell_new_surface;
    wl_signal_add(&layer_shell->wlr_layer_shell_v1->events.new_surface, &layer_shell->new_surface);

    layer_shell->destroy.notify = e_layer_shell_destroy;
    wl_signal_add(&layer_shell->wlr_layer_shell_v1->events.destroy, &layer_shell->destroy);

    return layer_shell;
}