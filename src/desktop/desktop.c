#include "desktop/desktop.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include "input/seat.h"

#include "desktop/tree/workspace.h"
#include "desktop/layer_shell.h"
#include "desktop/output.h"

#include "util/list.h"
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
    desktop->layers.unmanaged = wlr_scene_tree_create(&desktop->scene->tree);

    desktop->pending = wlr_scene_tree_create(&desktop->scene->tree);
    wlr_scene_node_set_enabled(&desktop->pending->node, false);
}

struct e_desktop* e_desktop_create(struct wl_display* display, struct wlr_compositor* compositor, struct e_config* config)
{
    assert(display && compositor && config);

    struct e_desktop* desktop = calloc(1, sizeof(*desktop));

    if (desktop == NULL)
    {
        e_log_error("failed to allocate desktop");
        return NULL;
    }
    
    desktop->display = display;
    desktop->config = config;

    wl_list_init(&desktop->outputs);

    //wlroots utility for working with arrangement of screens in a physical layout
    desktop->output_layout = wlr_output_layout_create(display);

    e_desktop_init_scene(desktop);

    //create 3 workspaces for desktop
    e_list_init(&desktop->workspaces, 3);

    for (int i = 0; i < 3; i++)
    {
        struct e_workspace* workspace = e_workspace_create(desktop);

        if (workspace != NULL)
            e_list_add(&desktop->workspaces, workspace);
        else
            e_log_error("e_desktop_create: failed to create workspace %i", i + 1);
    }
    
    wl_list_init(&desktop->views);

    //input device management
    desktop->seat = e_seat_create(display, desktop, desktop->output_layout, "seat0");

    return desktop;
}

/* outputs */

static void e_desktop_init_output(struct e_desktop* desktop, struct e_output* output)
{
    assert(desktop && output && output->layout);

    if (output->active_workspace == NULL)
    {
        // set active workspace to first inactive workspace in desktop
        for (int i = 0; i < desktop->workspaces.count; i++)
        {
            struct e_workspace* workspace = e_list_at(&desktop->workspaces, i);

            if (workspace != NULL && !workspace->active)
            {
                e_output_display_workspace(output, workspace);
                break;
            }
        }

        //just create new workspace for now
        if (output->active_workspace == NULL)
        {
            struct e_workspace* workspace = e_workspace_create(desktop);

            if (workspace != NULL)
                e_output_display_workspace(output, workspace);
            else
                e_log_error("e_desktop_init_output: failed to create workspace");
        }
    }

    e_output_arrange(output);
}

// Adds the given output to the given desktop and handles its layout for it.
// Returns desktop output on success, otherwise NULL on fail.
struct e_output* e_desktop_add_output(struct e_desktop* desktop, struct wlr_output* wlr_output)
{
    assert(desktop && wlr_output);

    struct e_output* output = e_output_create(desktop, wlr_output);

    wl_list_insert(&desktop->outputs, &output->link);

    //output layout auto adds wl_output to the display, allows wl clients to find out information about the display
    //TODO: allow configuring the arrangement of outputs in the layout
    //TODO: check for possible memory allocation error?
    struct wlr_output_layout_output* layout_output = wlr_output_layout_add_auto(desktop->output_layout, output->wlr_output);
    struct wlr_scene_output* scene_output = wlr_scene_output_create(desktop->scene, output->wlr_output);
    wlr_scene_output_layout_add_output(desktop->scene_layout, layout_output, scene_output);
    output->layout = desktop->output_layout;

    e_desktop_init_output(desktop, output);

    return output;
}

// Get output at specified index.
// Returns NULL on fail.
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

static void disable_output(struct e_output* output)
{
    assert(output);

    struct wlr_output_state state;
    wlr_output_state_init(&state);

    wlr_output_state_set_enabled(&state, false);

    //apply new output state
    wlr_output_commit_state(output->wlr_output, &state);
    wlr_output_state_finish(&state);
}

static void e_desktop_remove_output(struct e_desktop* desktop, struct e_output* output)
{
    assert(desktop && output);

    disable_output(output);

    e_output_display_workspace(output, NULL);

    wlr_output_layout_remove(desktop->output_layout, output->wlr_output);

    wl_list_remove(&output->link);
}

/* scene */

// Finds the scene surface at the specified layout coords in given scene graph.
// Also translates the layout coords to the surface coords if not NULL. (sx, sy)
// NULL for sx & sy is allowed.
// Returns NULL if nothing is found.
struct wlr_scene_surface* e_desktop_scene_surface_at(struct wlr_scene_node* node, double lx, double ly, double* sx, double* sy)
{
    assert(node);

    if (sx != NULL)
        *sx = 0.0;

    if (sy != NULL)
        *sy = 0.0;

    double nx, ny = 0.0;
    struct wlr_scene_node* snode = wlr_scene_node_at(node, lx, ly, &nx, &ny);

    if (snode == NULL || snode->type != WLR_SCENE_NODE_BUFFER)
        return NULL;

    struct wlr_scene_buffer* buffer = wlr_scene_buffer_from_node(snode);
    struct wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(buffer);

    if (scene_surface == NULL)
        return NULL;

    if (sx != NULL)
        *sx = nx;

    if (sy != NULL)
        *sy = ny;

    return scene_surface;
}

/* destruction */

void e_desktop_destroy(struct e_desktop* desktop)
{
    assert(desktop);

    //disable & then remove outputs
    struct e_output* output;
    struct e_output* tmp;

    wl_list_for_each_safe(output, tmp, &desktop->outputs, link)
    {
        e_desktop_remove_output(desktop, output);
    }

    //destroy all workspaces
    for (int i = 0; i < desktop->workspaces.count; i++)
    {
        struct e_workspace* workspace = e_list_at(&desktop->workspaces, i);

        if (workspace != NULL)
            e_workspace_destroy(workspace);
    }

    e_list_fini(&desktop->workspaces);
    
    //destroy root node
    wlr_scene_node_destroy(&desktop->scene->tree.node);

    wl_list_remove(&desktop->outputs);

    wl_list_remove(&desktop->views);

    free(desktop);
}
