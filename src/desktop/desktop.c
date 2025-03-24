#include "desktop/desktop.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "input/seat.h"
#include "desktop/layers/layer_surface.h"

#include "util/log.h"

// creates scene and scene layer trees
static void e_desktop_init_scene(struct e_desktop* desktop)
{
    assert(desktop);

    //handles all rendering & damage tracking, 
    //use this to add renderable things to the scene graph 
    //and then call wlr_scene_commit_output to render the frame
    desktop->scene = wlr_scene_create();
    desktop->scene_layout = wlr_scene_attach_output_layout(desktop->scene, desktop->output_layout);

    //create scene trees for all layers

    desktop->layers.background = wlr_scene_tree_create(&desktop->scene->tree);
    desktop->layers.bottom = wlr_scene_tree_create(&desktop->scene->tree);
    desktop->layers.tiling = wlr_scene_tree_create(&desktop->scene->tree);
    desktop->layers.floating = wlr_scene_tree_create(&desktop->scene->tree);
    desktop->layers.top = wlr_scene_tree_create(&desktop->scene->tree);
    desktop->layers.overlay = wlr_scene_tree_create(&desktop->scene->tree);

    desktop->pending = wlr_scene_tree_create(&desktop->scene->tree);
    wlr_scene_node_set_enabled(&desktop->pending->node, false);
}

struct e_desktop* e_desktop_create(struct wl_display* display, struct wlr_compositor* compositor, struct e_config* config)
{
    assert(display && compositor && config);

    struct e_desktop* desktop = calloc(1, sizeof(*desktop));
    
    desktop->display = display;
    desktop->config = config;

    wl_list_init(&desktop->outputs);

    //wlroots utility for working with arrangement of screens in a physical layout
    desktop->output_layout = wlr_output_layout_create(display);

    e_desktop_init_scene(desktop);
    
    wl_list_init(&desktop->windows);
    wl_list_init(&desktop->layer_surfaces);

    //input device management
    desktop->seat = e_seat_create(display, desktop, desktop->output_layout, "seat0");

    return desktop;
}

/* outputs */

static void e_desktop_init_output(struct e_desktop* desktop, struct e_output* output)
{
    assert(desktop && output && output->layout);

    //give output some pointers to the scene's layers
    output->layers.background = desktop->layers.background;
    output->layers.bottom = desktop->layers.bottom;
    output->layers.tiling = desktop->layers.tiling;
    output->layers.floating = desktop->layers.floating;
    output->layers.top = desktop->layers.top;
    output->layers.overlay = desktop->layers.overlay;

    //create output's root container
    if (output->root_tiling_container == NULL)
        output->root_tiling_container = e_container_create(output->layers.tiling, E_TILING_MODE_HORIZONTAL);

    if (output->root_floating_container == NULL)
        output->root_floating_container = e_container_create(output->layers.floating, E_TILING_MODE_NONE);

    e_output_arrange(output);
}

//adds the given output to the given scene and handles its layout for it
void e_desktop_add_output(struct e_desktop* desktop, struct e_output* output)
{
    assert(desktop && output);

    wl_list_insert(&desktop->outputs, &output->link);

    //output layout auto adds wl_output to the display, allows wl clients to find out information about the display
    //TODO: allow configuring the arrangement of outputs in the layout
    //TODO: check for possible memory allocation error?
    struct wlr_output_layout_output* layout_output = wlr_output_layout_add_auto(desktop->output_layout, output->wlr_output);
    struct wlr_scene_output* scene_output = wlr_scene_output_create(desktop->scene, output->wlr_output);
    wlr_scene_output_layout_add_output(desktop->scene_layout, layout_output, scene_output);
    output->layout = desktop->output_layout;

    e_desktop_init_output(desktop, output);
}

//get output at specified index, returns NULL if failed
struct e_output* e_desktop_get_output(struct e_desktop* desktop, int index)
{
    assert(desktop);

    if (wl_list_empty(&desktop->outputs))
        return NULL;

    if (index < 0 || index >= wl_list_length(&desktop->outputs))
        return NULL;

    struct wl_list* pos = desktop->outputs.next; //first output is in next, as desktop->outputs is contained in e_desktop (index 0)
    
    //go further in the list until we reach our destination
    for (int i = 0; i < index; i++)
        pos = desktop->outputs.next;

    struct e_output* output = wl_container_of(pos, output, link);

    return output;
}

static void e_desktop_remove_output(struct e_desktop* desktop, struct e_output* output)
{
    assert(desktop && output);

    if (output->root_tiling_container != NULL)
    {
        e_container_destroy(output->root_tiling_container);
        output->root_tiling_container = NULL;
    }

    if (output->root_floating_container != NULL)
    {
        e_container_destroy(output->root_floating_container);
        output->root_floating_container = NULL;
    }

    output->layers.background = NULL;
    output->layers.bottom = NULL;
    output->layers.tiling = NULL;
    output->layers.floating = NULL;
    output->layers.top = NULL;
    output->layers.overlay = NULL;

    wlr_output_layout_remove(desktop->output_layout, output->wlr_output);

    wl_list_remove(&output->link);
}

/* scene */

//returns a wlr_surface pointer at the specified layout coords, 
//also outs the surface's node, and translates the layout coords to the surface coords
struct wlr_surface* e_desktop_wlr_surface_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_scene_node** snode, double* sx, double* sy)
{
    if (node == NULL || snode == NULL || sx == NULL || sy == NULL)
    {
        e_log_error("e_scene_wlr_surface_at: *node, **snode, *sx, or *sy is NULL");
        abort();
    }

    *snode = wlr_scene_node_at(node, lx, ly, sx, sy);

    if (*snode == NULL || (*snode)->type != WLR_SCENE_NODE_BUFFER)
        return NULL;

    struct wlr_scene_buffer* buffer = wlr_scene_buffer_from_node(*snode);
    struct wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(buffer);

    if (scene_surface == NULL)
        return NULL;

    return scene_surface->surface;
}

/* layers */

void e_desktop_arrange_layer(struct e_desktop* desktop, struct wlr_output* wlr_output, enum zwlr_layer_shell_v1_layer layer)
{
    assert(desktop && wlr_output);

    struct wlr_box full_area = {0, 0, 0, 0};
    wlr_output_effective_resolution(wlr_output, &full_area.width, &full_area.height);

    struct wlr_box remaining_area = {full_area.x, full_area.y, full_area.width, full_area.height};

    //configure each layer surface in this output & layer
    //TODO: remaining area might become too small, I'm not sure what to do in those edge cases yet
    struct e_layer_surface* layer_surface;
    wl_list_for_each(layer_surface, &desktop->layer_surfaces, link)
    {
        //is this surface on this output & layer? if so, then configure and update remaining area
        if (e_layer_surface_get_wlr_output(layer_surface) == wlr_output && e_layer_surface_get_layer(layer_surface) == layer)
            e_layer_surface_configure(layer_surface, &full_area, &remaining_area);
    }
}

void e_desktop_arrange_all_layers(struct e_desktop* desktop, struct wlr_output* wlr_output)
{
    assert(desktop && wlr_output);

    e_desktop_arrange_layer(desktop, wlr_output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND);
    e_desktop_arrange_layer(desktop, wlr_output, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM);
    e_desktop_arrange_layer(desktop, wlr_output, ZWLR_LAYER_SHELL_V1_LAYER_TOP);
    e_desktop_arrange_layer(desktop, wlr_output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY);
}

//get topmost layer surface that requests exclusive focus, may be NULL
struct e_layer_surface* e_desktop_get_exclusive_topmost_layer_surface(struct e_desktop* desktop)
{
    if (wl_list_empty(&desktop->layer_surfaces))
        return NULL;

    struct e_layer_surface* topmost_layer_surface = NULL;
    struct e_layer_surface* layer_surface;

    wl_list_for_each(layer_surface, &desktop->layer_surfaces, link)
    {
        struct wlr_layer_surface_v1* wlr_layer_surface_v1 = layer_surface->scene_layer_surface_v1->layer_surface;

        //must request exclusive focus
        if (wlr_layer_surface_v1->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
            continue;

        //highest layer lives
        if (topmost_layer_surface == NULL)
            topmost_layer_surface = layer_surface;
        else if (wlr_layer_surface_v1->current.layer > e_layer_surface_get_layer(topmost_layer_surface))
            topmost_layer_surface = layer_surface;
    }

    return topmost_layer_surface;
}

/* destruction */

static void e_output_disable(struct e_output* output)
{
    assert(output);

    struct wlr_output_state state;
    wlr_output_state_init(&state);

    wlr_output_state_set_enabled(&state, false);

    //apply new output state
    wlr_output_commit_state(output->wlr_output, &state);
    wlr_output_state_finish(&state);
}

void e_desktop_destroy(struct e_desktop* desktop)
{
    assert(desktop);

    //disable & then remove outputs
    struct e_output* output;
    struct e_output* tmp;

    wl_list_for_each_safe(output, tmp, &desktop->outputs, link)
    {
        e_output_disable(output);

        e_desktop_remove_output(desktop, output);
    }
    
    //destroy root node
    wlr_scene_node_destroy(&desktop->scene->tree.node);

    wl_list_remove(&desktop->outputs);

    wl_list_remove(&desktop->windows);
    wl_list_remove(&desktop->layer_surfaces);

    free(desktop);
}