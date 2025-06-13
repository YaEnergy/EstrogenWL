#include "desktop/tree/workspace.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>

#include "desktop/tree/container.h"
#include "desktop/tree/node.h"

#include "desktop/views/view.h"

#include "desktop/output.h"

#include "util/list.h"
#include "util/log.h"

#include "protocols/cosmic-workspace-v1.h"

#include "server.h"

#define NEW_SCENE_TREE(tree, parent, type, data) tree = wlr_scene_tree_create(parent); e_node_desc_create(&tree->node, type, data);

// Create a new workspace for an output.
// Returns NULL on fail.
struct e_workspace* e_workspace_create(struct e_output* output)
{
    assert(output);

    if (output == NULL)
    {
        e_log_error("e_workspace_create: no output given!");
        return NULL;
    }

    struct e_workspace* workspace = calloc(1, sizeof(*workspace));

    if (workspace == NULL)
    {
        e_log_error("e_workspace_create: failed to allocate workspace!");
        return NULL;
    }

    workspace->output = output;
    workspace->fullscreen_view = NULL;

    workspace->root_tiling_container = e_tree_container_create(E_TILING_MODE_HORIZONTAL);

    if (workspace->root_tiling_container == NULL)
    {
        free(workspace);

        e_log_error("e_workspace_create: failed to allocate root tiling container!");
        return NULL;
    }

    workspace->cosmic_handle = e_cosmic_workspace_create(output->workspace_group.cosmic_handle);

    if (workspace->cosmic_handle == NULL)
    {
        e_tree_container_destroy(workspace->root_tiling_container);
        free(workspace);
        
        e_log_error("e_workspace_create: failed to create cosmic workspace handle!");
        return NULL;
    }

    //layer trees
    NEW_SCENE_TREE(workspace->layers.floating, output->layers.floating, E_NODE_DESC_WORKSPACE, workspace);
    NEW_SCENE_TREE(workspace->layers.tiling, output->layers.tiling, E_NODE_DESC_WORKSPACE, workspace);
    NEW_SCENE_TREE(workspace->layers.fullscreen, output->layers.overlay, E_NODE_DESC_WORKSPACE, workspace);

    e_list_init(&workspace->floating_views, 10);

    e_workspace_set_activated(workspace, false);

    return workspace;
}

// Enable/disable workspace trees.
void e_workspace_set_activated(struct e_workspace* workspace, bool activated)
{
    assert(workspace);

    if (workspace == NULL)
    {
        e_log_error("e_workspace_set_activated: workspace is NULL!");
        return;
    }

    workspace->active = activated;
    e_workspace_update_tree_visibility(workspace);

    e_cosmic_workspace_set_active(workspace->cosmic_handle, activated);
    e_cosmic_workspace_set_hidden(workspace->cosmic_handle, !activated);
}

// Arranges a workspace's children to fit within the given area.
void e_workspace_arrange(struct e_workspace* workspace, struct wlr_box full_area, struct wlr_box tiled_area)
{
    if (workspace == NULL)
    {
        e_log_error("e_workspace_arrange: workspace is NULL!");
        return;
    }

    workspace->full_area = full_area;
    workspace->tiled_area = tiled_area;

    //TODO: move trees, don't use x y in configure

    if (workspace->fullscreen_view != NULL)
        e_view_configure(workspace->fullscreen_view, full_area.x, full_area.y, full_area.width, full_area.height);

    e_container_configure(&workspace->root_tiling_container->base, tiled_area.x, tiled_area.y, tiled_area.width, tiled_area.height);
}

// Update visiblity of workspace trees.
void e_workspace_update_tree_visibility(struct e_workspace* workspace)
{
    if (workspace == NULL)
    {
        e_log_error("e_workspace_update_trees: workspace is NULL!");
        return;
    }

    if (workspace->active)
    {
        bool fullscreen = (workspace->fullscreen_view != NULL);

        wlr_scene_node_set_enabled(&workspace->layers.floating->node, !fullscreen);
        wlr_scene_node_set_enabled(&workspace->layers.tiling->node, !fullscreen);
        wlr_scene_node_set_enabled(&workspace->layers.fullscreen->node, fullscreen);
    }
    else 
    {
        wlr_scene_node_set_enabled(&workspace->layers.floating->node, false);
        wlr_scene_node_set_enabled(&workspace->layers.tiling->node, false);
        wlr_scene_node_set_enabled(&workspace->layers.fullscreen->node, false);
    }
}

// Returns NULL on fail.
static struct e_workspace* e_workspace_try_from_node(struct wlr_scene_node* node)
{
    if (node == NULL)
        return NULL;

    //data is either NULL or e_node_desc
    if (node->data == NULL)
        return NULL;

    struct e_node_desc* node_desc = node->data;

    if (node_desc->type == E_NODE_DESC_WORKSPACE)
        return node_desc->data;
    else
        return NULL;
}

// Get workspace from node ancestors.
// Returns NULL on fail.
struct e_workspace* e_workspace_try_from_node_ancestors(struct wlr_scene_node* node)
{
    if (node == NULL)
        return NULL;

    struct e_workspace* workspace = e_workspace_try_from_node(node);

    if (workspace != NULL)
        return workspace;

    //keep going upwards in the tree until we find a workspace (in which case we return it), or reach the root of the tree (no parent)
    while (node->parent != NULL)
    {
        //go to parent node
        node = &node->parent->node;

        struct e_workspace* workspace = e_workspace_try_from_node(node);

        if (workspace != NULL)
            return workspace;
    }

    return NULL;
}

// Destroy the workspace.
void e_workspace_destroy(struct e_workspace* workspace)
{
    assert(workspace);

    if (workspace == NULL)
    {
        e_log_error("e_workspace_destroy: workspace is NULL!");
        return;
    }
    e_cosmic_workspace_remove(workspace->cosmic_handle);

    e_tree_container_destroy(workspace->root_tiling_container);
    workspace->root_tiling_container = NULL;

    // Destroy workspace layer trees
    wlr_scene_node_destroy(&workspace->layers.floating->node);
    wlr_scene_node_destroy(&workspace->layers.tiling->node);
    wlr_scene_node_destroy(&workspace->layers.fullscreen->node);

    e_list_fini(&workspace->floating_views);

    free(workspace);
}
