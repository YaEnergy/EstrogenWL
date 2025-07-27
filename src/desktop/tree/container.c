#include "desktop/tree/container.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/util/box.h>

#include "desktop/tree/workspace.h"
#include "desktop/tree/node.h"
#include "desktop/views/view.h"

#include "server.h"

#include "util/wl_macros.h"
#include "util/list.h"
#include "util/log.h"

bool e_container_init(struct e_container* container, enum e_container_type type, struct e_server* server)
{
    assert(container);

    container->server = server;

    container->tree = wlr_scene_tree_create(server->pending);

    if (container->tree == NULL)
        return false;

    if (e_node_desc_create(&container->tree->node, E_NODE_DESC_CONTAINER, container) == NULL)
    {
        wlr_scene_node_destroy(&container->tree->node);
        return false;
    }

    container->type = type;

    container->workspace = NULL;
    container->parent = NULL;

    container->area = (struct wlr_box){0, 0, 0, 0};

    wl_signal_init(&container->events.destroy);

    return true;
}

void e_container_fini(struct e_container* container)
{
    assert(container);

    if (container->parent != NULL)
        e_tree_container_remove_container(container->parent, container);

    wlr_scene_node_destroy(&container->tree->node);
    container->tree = NULL;
}

bool e_container_is_tiled(struct e_container* container)
{
    assert(container);

    return container->parent != NULL;
}

void e_container_set_workspace(struct e_container* container, struct e_workspace* workspace)
{
    assert(container);

    container->workspace = workspace;

    switch (container->type)
    {
        case E_CONTAINER_TREE:
            for (int i = 0; i < container->tree_container->children.count; i++)
            {
                struct e_container* child = e_list_at(&container->tree_container->children, i);
                
                if (child != NULL)
                    e_container_set_workspace(child, workspace);
            }
            break;
        case E_CONTAINER_VIEW:
            //TODO: update view output & layering
            break;
    }
}

// Sets the parent of a container.
// Parent may be NULL.
// Returns true on success, false on fail.
bool e_container_set_parent(struct e_container* container, struct e_tree_container* parent)
{
    assert(container);

    //don't set to same parent
    if (container->parent == parent)
        return false;

    if (container->parent != NULL)
    {
        bool remove_success = e_tree_container_remove_container(container->parent, container);

        if (!remove_success)
            return false;
    }

    if (parent != NULL)
        return e_tree_container_add_container(parent, container);
    else
        return true;
}

static void arrange_tree_children(struct e_tree_container* tree_container, struct wlr_box area)
{
    assert(tree_container);

    float percentageStart = 0.0f;

    for (int i = 0; i < tree_container->children.count; i++)
    {
        struct e_container* child_container = e_list_at(&tree_container->children, i);

        if (child_container == NULL)
            continue;

        struct wlr_box child_area = {area.x, area.y, area.width, area.height};

        switch (tree_container->tiling_mode)
        {
            case E_TILING_MODE_HORIZONTAL:
                child_area.x += area.width * percentageStart;
                child_area.width *= child_container->percentage;
                break;
            case E_TILING_MODE_VERTICAL:
                child_area.y += area.height * percentageStart;
                child_area.height *= child_container->percentage;
                break;
            default:
                e_log_error("e_tree_container_arrange: unknown container tiling mode!");
                continue;
        }

        percentageStart += child_container->percentage;

        e_container_arrange(child_container, child_area);
    }
}

static void arrange_view(struct e_view_container* view_container, struct wlr_box area)
{
    assert(view_container);

    //TODO: fullscreen
    view_container->view_pending = (struct wlr_box){
        .x = area.x,
        .y = area.y,
        .width = (area.width > 0) ? area.width : 1,
        .height = (area.height > 0) ? area.height : 1
    };

    if (view_container->view != NULL)
        e_view_configure(view_container->view, view_container->view_pending.x, view_container->view_pending.y, view_container->view_pending.width, view_container->view_pending.height);
}

// Arrange container within given area.
void e_container_arrange(struct e_container* container, struct wlr_box area)
{
    assert(container);

    container->area = area;

    switch (container->type)
    {
        case E_CONTAINER_TREE:
            arrange_tree_children(container->tree_container, area);
            break;
        case E_CONTAINER_VIEW:
            arrange_view(container->view_container, area);
            break;
    }
}

void e_container_rearrange(struct e_container* container)
{
    assert(container);

    e_container_arrange(container, container->area);
}

void e_container_raise_to_top(struct e_container* container)
{
    assert(container);

    wlr_scene_node_raise_to_top(&container->tree->node);
}

// Returns NULL on fail.
static struct e_container* e_container_try_from_node(struct wlr_scene_node* node)
{
    if (node == NULL)
        return NULL;

    //data is either NULL or e_node_desc
    if (node->data == NULL)
        return NULL;

    struct e_node_desc* node_desc = node->data;

    return e_container_try_from_e_node_desc(node_desc);
}

// Returns NULL on fail.
struct e_container* e_container_try_from_node_ancestors(struct wlr_scene_node* node)
{
    if (node == NULL)
        return NULL;

    struct e_container* container = e_container_try_from_node(node);

    if (container != NULL)
        return container;

    //keep going upwards in the tree until we find a view container (in which case we return it), or reach the root of the tree (no parent)
    while (node->parent != NULL)
    {
        //go to parent node
        node = &node->parent->node;

        container = e_container_try_from_node(node);

        if (container != NULL)
            return container;
    }

    return NULL;
}

// Finds the container at the specified layout coords in given scene graph.
// Returns NULL on fail.
struct e_container* e_container_at(struct wlr_scene_node* node, double lx, double ly)
{
    if (node == NULL)
        return NULL;

    struct wlr_scene_node* node_at = wlr_scene_node_at(node, lx, ly, NULL, NULL);
    
    if (node_at != NULL)
        return e_container_try_from_node_ancestors(node_at);
    else
        return NULL;
}

// Tree container functions

// Creates a tree container.
// Returns NULL on fail.
struct e_tree_container* e_tree_container_create(struct e_server* server, enum e_tiling_mode tiling_mode)
{
    struct e_tree_container* tree_container = calloc(1, sizeof(*tree_container));

    if (tree_container == NULL)
    {
        e_log_error("e_tree_container_create: failed to alloc tree_container");
        return NULL;
    }

    if (!e_container_init(&tree_container->base, E_CONTAINER_TREE, server))
    {
        e_log_error("e_tree_container_create: failed to init container");
        free(tree_container);
        return NULL;
    }

    e_list_init(&tree_container->children, 5);

    tree_container->base.tree_container = tree_container;
    tree_container->tiling_mode = tiling_mode;

    tree_container->destroying = false;

    return tree_container;
}

// Inserts a container in tree container at given index.
// Returns true on success, false on fail.
bool e_tree_container_insert_container(struct e_tree_container* tree_container, struct e_container* container, int index)
{
    assert(tree_container && container);

    #if E_VERBOSE
    e_log_info("tree container insert container at index %i", index);
    #endif

    if (container->parent != NULL)
    {
        e_log_error("container is already inside a tree container!");
        return false;
    }

    //add to children, return false on fail
    if (!e_list_insert(&tree_container->children, container, index))
        return false;

    container->parent = tree_container;
    e_container_set_workspace(container, tree_container->base.workspace);

    //TODO: allow having containers of different percentages
    for (int i = 0; i < tree_container->children.count; i++)
    {
        struct e_container* container = e_list_at(&tree_container->children, i);

        if (container != NULL)
            container->percentage = 1.0f / tree_container->children.count;
    }

    return true;
}

// Adds a container to a tree container.
// Returns true on success, false on fail.
bool e_tree_container_add_container(struct e_tree_container* tree_container, struct e_container* container)
{
    return e_tree_container_insert_container(tree_container, container, tree_container->children.count);
}

// Removes a container from a tree container.
// Returns true on success, false on fail.
bool e_tree_container_remove_container(struct e_tree_container* tree_container, struct e_container* container)
{
    assert(tree_container && container);

    #if E_VERBOSE
    e_log_info("tree container remove container");
    #endif

    if (container->parent != tree_container)
    {
        e_log_error("container is not a child of tree container!");
        return false;
    }

    container->parent = NULL;
    e_list_remove(&tree_container->children, container);

    //distribute container's percentage evenly across remaining children

    float percentage = container->percentage;

    for (int i = 0; i < tree_container->children.count; i++)
    {
        struct e_container* container = e_list_at(&tree_container->children, i);

        if (container != NULL)
            container->percentage += percentage / tree_container->children.count;
    }

    return true;
}

static void e_tree_container_clear_children(struct e_tree_container* tree_container)
{
    assert(tree_container);

    //no children to destroy, lmao
    if (tree_container->children.count == 0)
        return;

    struct e_list list = { 0 }; //struct e_container*
    e_list_init(&list, tree_container->children.count);

    for (int i = 0; i < tree_container->children.count; i++)
        e_list_add(&list, e_list_at(&tree_container->children, i));

    for (int i = 0; i < list.count; i++)
        e_container_destroy(e_list_at(&list, i));

    e_list_fini(&list);
}

static void e_tree_container_destroy(struct e_tree_container* tree_container)
{
    assert(tree_container);

    if (tree_container->destroying)
        return;

    tree_container->destroying = true;

    wl_signal_emit_mutable(&tree_container->base.events.destroy, NULL);

    e_tree_container_clear_children(tree_container);

    e_list_fini(&tree_container->children);
    
    e_container_fini(&tree_container->base);

    free(tree_container);
}

static void e_view_container_destroy(struct e_view_container* view_container)
{
    assert(view_container);

    if (view_container == NULL)
    {
        e_log_error("e_view_container_destroy: view_container is NULL!");
        return;
    }

    wl_signal_emit_mutable(&view_container->base.events.destroy, NULL);

    wl_list_remove(&view_container->link);

    //reparent view node before destroying container node, so we don't destroy the view's tree aswell
    wlr_scene_node_reparent(&view_container->view->tree->node, view_container->base.server->pending);

    SIGNAL_DISCONNECT(view_container->map);
    SIGNAL_DISCONNECT(view_container->unmap);

    SIGNAL_DISCONNECT(view_container->destroy);

    e_container_fini(&view_container->base);

    free(view_container);
}

void e_container_destroy(struct e_container* container)
{
    assert(container);

    switch (container->type)
    {
        case E_CONTAINER_TREE:
            e_tree_container_destroy(container->tree_container);
            break;
        case E_CONTAINER_VIEW:
            e_view_container_destroy(container->view_container);    
            break;
    }
}