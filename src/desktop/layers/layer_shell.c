#include "desktop/layers/layer_shell.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_layer_shell_v1.h>

#include "desktop/layers/layer_surface.h"
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
}

static void e_layer_shell_destroy(struct wl_listener* listener, void* data)
{
    struct e_layer_shell* layer_shell = wl_container_of(listener, layer_shell, destroy);

    wl_list_remove(&layer_shell->layer_surfaces);
    wl_list_remove(&layer_shell->new_surface.link);
    wl_list_remove(&layer_shell->destroy.link);

    free(layer_shell);
}

struct e_layer_shell* e_layer_shell_create(struct e_server* server)
{
    struct e_layer_shell* layer_shell = calloc(1, sizeof(struct e_layer_shell));
    layer_shell->server = server;

    layer_shell->wlr_layer_shell_v1 = wlr_layer_shell_v1_create(server->display, 4);

    wl_list_init(&layer_shell->layer_surfaces);

    //events

    layer_shell->new_surface.notify = e_layer_shell_new_surface;
    wl_signal_add(&layer_shell->wlr_layer_shell_v1->events.new_surface, &layer_shell->new_surface);

    layer_shell->destroy.notify = e_layer_shell_destroy;
    wl_signal_add(&layer_shell->wlr_layer_shell_v1->events.destroy, &layer_shell->destroy);

    return layer_shell;
}

//Maybe I should create these kinds of methods for tiling e_windows aswell?
void e_layer_shell_arrange_layer(struct e_layer_shell* layer_shell, struct wlr_output* wlr_output, enum zwlr_layer_shell_v1_layer layer)
{
    struct wlr_box full_area = {0, 0, 0, 0};
    wlr_output_effective_resolution(wlr_output, &full_area.width, &full_area.height);

    struct wlr_box remaining_area = {full_area.x, full_area.y, full_area.width, full_area.height};

    //configure each layer surface in this output & layer
    //TODO: remaining area might become too small, I'm not sure what to do in those edge cases yet
    struct e_layer_surface* layer_surface;
    wl_list_for_each(layer_surface, &layer_shell->layer_surfaces, link)
    {
        //is this surface on this output & layer? if so, then configure and update remaining area
        if (e_layer_surface_get_wlr_output(layer_surface) == wlr_output && e_layer_surface_get_layer(layer_surface) == layer)
        {
            e_layer_surface_configure(layer_surface, &full_area, &remaining_area);
        }
    }
}

void e_layer_shell_arrange_all_layers(struct e_layer_shell* layer_shell, struct wlr_output* wlr_output)
{
    e_layer_shell_arrange_layer(layer_shell, wlr_output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND);
    e_layer_shell_arrange_layer(layer_shell, wlr_output, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM);
    e_layer_shell_arrange_layer(layer_shell, wlr_output, ZWLR_LAYER_SHELL_V1_LAYER_TOP);
    e_layer_shell_arrange_layer(layer_shell, wlr_output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY);
}