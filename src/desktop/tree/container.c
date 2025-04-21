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
#include "util/list.h"
#include "util/log.h"

bool e_container_init(struct e_container* container, enum e_container_type type, void* data)
{
    assert(container && data);

    container->type = type;
    container->data = data;

    container->implementation.configure = NULL;
    container->implementation.destroy = NULL;

    return true;
}

void e_container_fini(struct e_container* container)
{
    assert(container);

    if (container->parent != NULL)
        e_tree_container_remove_container(container->parent, container);

    container->implementation.configure = NULL;
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

// Configure the container.
void e_container_configure(struct e_container* container, int x, int y, int width, int height)
{
    assert(container);

    if (container->implementation.configure != NULL)
        container->implementation.configure(container, x, y, width, height);
    else
        e_log_error("e_container_configure: not implemented!");
}

void e_container_destroy(struct e_container* container)
{
    assert(container);

    if (container->implementation.destroy != NULL)
        container->implementation.destroy(container);
}

// Tree container functions

static void e_container_configure_tree_container(struct e_container* container, int x, int y, int width, int height)
{
    assert(container);

    container->area = (struct wlr_box){x, y, width, height};

    struct e_tree_container* tree_container = container->data;
    e_tree_container_arrange(tree_container);
}

static void e_container_destroy_tree_container(struct e_container* container)
{
    assert(container);

    e_tree_container_destroy(container->data);
}

// Creates a tree container.
// Returns NULL on fail.
struct e_tree_container* e_tree_container_create(enum e_tiling_mode tiling_mode)
{
    struct e_tree_container* tree_container = calloc(1, sizeof(*tree_container));

    if (tree_container == NULL)
    {
        e_log_error("failed to alloc tree_container");
        return NULL;
    }

    e_list_init(&tree_container->children, 5);

    e_container_init(&tree_container->base, E_CONTAINER_TREE, tree_container);
    tree_container->tiling_mode = tiling_mode;

    tree_container->destroying = false;

    //implementation
    
    tree_container->base.implementation.configure = e_container_configure_tree_container;
    tree_container->base.implementation.destroy = e_container_destroy_tree_container;

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

    //TODO: allow having containers of different percentages
    for (int i = 0; i < tree_container->children.count; i++)
    {
        struct e_container* container = e_list_at(&tree_container->children, i);
        container->percentage = 1.0f / tree_container->children.count;
    }

    return true;
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

    //add to children, return false on fail
    if (!e_list_add(&tree_container->children, container))
        return false;

    container->parent = tree_container;

    //TODO: allow having containers of different percentages
    for (int i = 0; i < tree_container->children.count; i++)
    {
        struct e_container* container = e_list_at(&tree_container->children, i);
        container->percentage = 1.0f / tree_container->children.count;
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
    e_list_remove(&tree_container->children, container);

    if (tree_container->children.count == 0 && tree_container->base.parent != NULL)
    {
        e_tree_container_destroy(tree_container);
        return true;
    }

    //TODO: allow having containers of different percentages
    for (int i = 0; i < tree_container->children.count; i++)
    {
        struct e_container* container = e_list_at(&tree_container->children, i);
        container->percentage = 1.0f / tree_container->children.count;
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

    for (int i = 0; i < tree_container->children.count; i++)
    {
        struct e_container* child_container = e_list_at(&tree_container->children, i);
        struct wlr_box child_area = {tree_container->base.area.x, tree_container->base.area.y, tree_container->base.area.width, tree_container->base.area.height};

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

void e_tree_container_destroy(struct e_tree_container* tree_container)
{
    assert(tree_container);

    if (tree_container->destroying)
        return;

    tree_container->destroying = true;

    e_tree_container_clear_children(tree_container);

    e_list_fini(&tree_container->children);
    
    e_container_fini(&tree_container->base);

    free(tree_container);
}
