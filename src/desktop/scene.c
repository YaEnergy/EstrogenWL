#include "desktop/scene.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_compositor.h>

#include "desktop/tree/container.h"

#include "output.h"

#include "util/log.h"

struct e_scene* e_scene_create(struct wl_display* display)
{
    struct e_scene* scene = calloc(1, sizeof(struct e_scene));

    //wlroots utility for working with arrangement of screens in a physical layout
    scene->output_layout = wlr_output_layout_create(display);

    //handles all rendering & damage tracking, 
    //use this to add renderable things to the scene graph 
    //and then call wlr_scene_commit_output to render the frame
    scene->wlr_scene = wlr_scene_create();
    scene->scene_layout = wlr_scene_attach_output_layout(scene->wlr_scene, scene->output_layout);

    //create scene trees for all layers

    scene->layers.background = wlr_scene_tree_create(&scene->wlr_scene->tree);
    scene->layers.bottom = wlr_scene_tree_create(&scene->wlr_scene->tree);
    scene->layers.tiling = wlr_scene_tree_create(&scene->wlr_scene->tree);
    scene->layers.floating = wlr_scene_tree_create(&scene->wlr_scene->tree);
    scene->layers.top = wlr_scene_tree_create(&scene->wlr_scene->tree);
    scene->layers.overlay = wlr_scene_tree_create(&scene->wlr_scene->tree);

    scene->pending = wlr_scene_tree_create(&scene->wlr_scene->tree);
    wlr_scene_node_set_enabled(&scene->pending->node, false);

    wl_list_init(&scene->outputs);

    wl_list_init(&scene->windows);

    return scene;
}

//get output at specified index, returns NULL if failed
struct e_output* e_scene_get_output(struct e_scene* scene, int index)
{
    if (wl_list_empty(&scene->outputs))
        return NULL;

    struct e_output* output;
    int i = 0;
    wl_list_for_each(output, &scene->outputs, link)
    {
        if (i == index)
            return output;

        i++;
    }

    return NULL;
}

static void e_scene_init_output(struct e_scene* scene, struct e_output* output)
{
    assert(scene && output && output->layout);

    //give output some pointers to the scene's layers
    output->layers.background = scene->layers.background;
    output->layers.bottom = scene->layers.bottom;
    output->layers.tiling = scene->layers.tiling;
    output->layers.floating = scene->layers.floating;
    output->layers.top = scene->layers.top;
    output->layers.overlay = scene->layers.overlay;

    //create output's root container
    if (output->root_tiling_container == NULL)
        output->root_tiling_container = e_container_create(output->layers.tiling, E_TILING_MODE_HORIZONTAL);

    if (output->root_floating_container == NULL)
        output->root_floating_container = e_container_create(output->layers.floating, E_TILING_MODE_NONE);

    e_output_arrange(output);
}

void e_scene_add_output(struct e_scene* scene, struct e_output* output)
{
    wl_list_insert(&scene->outputs, &output->link);

    //output layout auto adds wl_output to the display, allows wl clients to find out information about the display
    //TODO: allow configuring the arrangement of outputs in the layout
    //TODO: check for possible memory allocation error?
    struct wlr_output_layout_output* layout_output = wlr_output_layout_add_auto(scene->output_layout, output->wlr_output);
    struct wlr_scene_output* scene_output = wlr_scene_output_create(scene->wlr_scene, output->wlr_output);
    wlr_scene_output_layout_add_output(scene->scene_layout, layout_output, scene_output);
    output->layout = scene->output_layout;

    e_scene_init_output(scene, output);
}

static void e_scene_remove_output(struct e_scene* scene, struct e_output* output)
{
    assert(scene && output);

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

    wlr_output_layout_remove(scene->output_layout, output->wlr_output);

    wl_list_remove(&output->link);
}

struct wlr_surface* e_scene_wlr_surface_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_scene_node** snode, double* sx, double* sy)
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

void e_scene_destroy(struct e_scene* scene)
{
    //disable & then remove outputs
    struct e_output* output;
    struct e_output* tmp;

    wl_list_for_each_safe(output, tmp, &scene->outputs, link)
    {
        e_output_disable(output);

        e_scene_remove_output(scene, output);
    }
    
    //destroy root node
    wlr_scene_node_destroy(&scene->wlr_scene->tree.node);

    wl_list_remove(&scene->outputs);

    free(scene);
}