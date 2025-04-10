#include "desktop/tree/container.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

#include "desktop/tree/node.h"
#include "util/log.h"

bool e_container_init(struct e_container* container, struct wlr_scene_tree* parent, enum e_container_type type, void* data)
{
    assert(container && parent && data);

    container->tree = wlr_scene_tree_create(parent);
    container->tree->node.data = e_node_desc_create(&container->tree->node, E_NODE_DESC_CONTAINER, container);

    container->type = type;
    container->data = data;

    container->implementation.configure = NULL;
    container->implementation.destroy = NULL;

    wl_list_init(&container->link);

    return true;
}

void e_container_fini(struct e_container* container)
{
    assert(container);

    if (container->parent != NULL)
        e_tree_container_remove_container(container->parent, container);

    container->implementation.configure = NULL;

    if (container->tree != NULL)
    {
        wlr_scene_node_destroy(&container->tree->node);
        container->tree = NULL;
    }
}

// Sets the parent of a container.
// Returns true on success, false on fail.
bool e_container_set_parent(struct e_container* container, struct e_tree_container* parent)
{
    assert(container && parent);

    //don't set to same parent
    if (container->parent == parent)
        return false;

    if (container->parent != NULL)
    {
        bool remove_success = e_tree_container_remove_container(container->parent, container);

        if (!remove_success)
            return false;
    }

    return e_tree_container_add_container(parent, container);
}

void e_container_set_position(struct e_container* container, int lx, int ly)
{
    assert(container);

    wlr_scene_node_set_position(&container->tree->node, lx, ly);
    container->area.x = lx;
    container->area.y = ly;
}

// Configure the container.
// Returns configure serial, returns 0 if no serial was given.
uint32_t e_container_configure(struct e_container* container, int lx, int ly, int width, int height)
{
    assert(container);

    if (container->implementation.configure != NULL)
        return container->implementation.configure(container, lx, ly, width, height);
    else
        return 0;
}

void e_container_destroy(struct e_container* container)
{
    assert(container);

    if (container->implementation.destroy != NULL)
        container->implementation.destroy(container);
}

// Tree container functions

static uint32_t e_container_configure_tree_container(struct e_container* container, int lx, int ly, int width, int height)
{
    assert(container);

    wlr_scene_node_set_position(&container->tree->node, lx, ly);
    container->area = (struct wlr_box){lx, ly, width, height};

    struct e_tree_container* tree_container = container->data;
    e_tree_container_arrange(tree_container);

    return 0;
}

static void e_container_destroy_tree_container(struct e_container* container)
{
    assert(container);

    e_tree_container_destroy(container->data);
}

// Creates a tree container.
// Returns NULL on fail.
struct e_tree_container* e_tree_container_create(struct wlr_scene_tree* parent, enum e_tiling_mode tiling_mode)
{
    assert(parent);

    struct e_tree_container* tree_container = calloc(1, sizeof(*tree_container));

    if (tree_container == NULL)
    {
        e_log_error("failed to alloc tree_container");
        return NULL;
    }

    e_container_init(&tree_container->base, parent, E_CONTAINER_TREE, tree_container);
    tree_container->tiling_mode = tiling_mode;
    wl_list_init(&tree_container->children);

    tree_container->destroying = false;

    //implementation
    
    tree_container->base.implementation.configure = e_container_configure_tree_container;
    tree_container->base.implementation.destroy = e_container_destroy_tree_container;

    return tree_container;
}

// Adds a container to a tree container.
// Returns true on success, false on fail.
bool e_tree_container_add_container(struct e_tree_container* tree_container, struct e_container* container)
{
    assert(tree_container && container);

    #if E_VERBOSE
    e_log_info("tree container add container");
    #endif

    if (container->parent != NULL)
    {
        e_log_error("container is already inside a tree container!");
        return false;
    }

    wl_list_insert(&tree_container->children, &container->link);
    container->parent = tree_container;
    wlr_scene_node_reparent(&container->tree->node, tree_container->base.tree);

    int num_containers = wl_list_length(&tree_container->children);
    struct e_container* contained_container;
    wl_list_for_each(contained_container, &tree_container->children, link)
    {
        contained_container->percentage = 1.0f / (float)num_containers;
    }

    return true;
}

// Removes a container from a tree container.
// Tree container is destroyed when no children are left and has a parent.
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
    wl_list_remove(&container->link);

    if (wl_list_empty(&tree_container->children))
    {
        if (tree_container->base.parent != NULL)
            e_tree_container_destroy(tree_container);
        
        return true;
    }

    //distribute container's percentage evenly across the other children
    
    int num_containers = wl_list_length(&tree_container->children);
    struct e_container* contained_container;
    wl_list_for_each(contained_container, &tree_container->children, link)
    {
        contained_container->percentage += container->percentage / (float)num_containers;
    }

    return true;
}

// Arranges a tree container's children to fit within the container's area.
void e_tree_container_arrange(struct e_tree_container* tree_container)
{
    assert(tree_container);

    #if E_VERBOSE
    e_log_info("tree container arrange");
    #endif

    if (tree_container->tiling_mode == E_TILING_MODE_NONE)
        return;

    float percentageStart = 0.0f;

    struct e_container* child_container;
    wl_list_for_each(child_container, &tree_container->children, link)
    {
        struct wlr_box child_area = {0, 0, tree_container->base.area.width, tree_container->base.area.height};

        switch (tree_container->tiling_mode)
        {
            case E_TILING_MODE_HORIZONTAL:
                child_area.x = tree_container->base.area.width * percentageStart;
                child_area.width *= child_container->percentage;
                break;
            case E_TILING_MODE_VERTICAL:
                child_area.y = tree_container->base.area.height * percentageStart;
                child_area.height *= child_container->percentage;
                break;
            default:
                e_log_error("Unknown container tiling mode!");
                continue;
        }

        percentageStart += child_container->percentage;

        e_container_configure(child_container, child_area.x, child_area.y, child_area.width, child_area.height);
    }
}

void e_tree_container_destroy(struct e_tree_container* tree_container)
{
    assert(tree_container);

    if (tree_container->destroying)
        return;

    tree_container->destroying = true;

    //destroy children
    struct e_container* container = NULL;
    struct e_container* tmp = NULL;

    wl_list_for_each_safe(container, tmp, &tree_container->children, link)
    {
        e_container_destroy(container);
    }
    
    e_container_fini(&tree_container->base);

    free(tree_container);
}