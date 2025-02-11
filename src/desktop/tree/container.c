#include "desktop/tree/container.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

#include "desktop/tree/node.h"
#include "desktop/windows/window.h"
#include "util/log.h"

//TODO: return NULL and add error checks instead of aborting
//TODO: destroy container on window's destroy signal

static void e_container_destroy_node(struct wl_listener* listener, void* data)
{
    struct e_container* container = wl_container_of(listener, container, destroy);

    if (container->parent != NULL)
        e_container_remove_container(container->parent, container);

    container->tree = NULL;

    if (container->window != NULL)
        container->window->container = NULL;

    wl_list_init(&container->link); //just in case container->link wasn't init
    wl_list_remove(&container->link);

    wl_list_remove(&container->containers);

    wl_list_remove(&container->destroy.link);
    
    free(container);
}

struct e_container* e_container_create(struct wlr_scene_tree* parent, enum e_tiling_mode tiling_mode)
{
    assert(parent);

    struct e_container* container = calloc(1, sizeof(*container));

    if (container == NULL)
    {
        e_log_error("Failed to allocate e_container");
        abort();
    }

    container->parent = NULL;
    container->tree = wlr_scene_tree_create(parent);
    container->tree->node.data = e_node_desc_create(&container->tree->node, E_NODE_DESC_CONTAINER, container);
    container->tiling_mode = tiling_mode;
    container->destroying = false;

    wl_list_init(&container->link);
    wl_list_init(&container->containers);

    //events

    container->destroy.notify = e_container_destroy_node;
    wl_signal_add(&container->tree->node.events.destroy, &container->destroy);

    return container;
}

void e_container_add_container(struct e_container* container, struct e_container* child_container)
{
    assert(container && child_container);

    if (child_container->parent != NULL)
    {
        e_log_error("child container is already inside a container!");
        abort();
    }

    wl_list_insert(&container->containers, &child_container->link);
    child_container->parent = container;
    wlr_scene_node_reparent(&child_container->tree->node, container->tree);

    if (wl_list_empty(&container->containers))
        return;

    int num_containers = wl_list_length(&container->containers);
    struct e_container* contained_container;
    wl_list_for_each(contained_container, &container->containers, link)
    {
        contained_container->percentage = 1.0f / (float)num_containers;
    }
}

//Removes a container from a container
void e_container_remove_container(struct e_container* container, struct e_container* child_container)
{
    assert(container && child_container);

    if (child_container->parent != container)
    {
        e_log_error("child container is not a child container of container!");
        abort();
    }

    child_container->parent = NULL;
    wl_list_remove(&child_container->link);

    if (wl_list_empty(&container->containers))
        return;

    int num_containers  = wl_list_length(&container->containers);
    struct e_container* contained_container;
    wl_list_for_each(contained_container, &container->containers, link)
    {
        contained_container->percentage = 1.0f / (float)num_containers;
    }
}

//Sets the parent of a container
void e_container_set_parent(struct e_container* container, struct e_container* parent)
{
    assert(container && parent);

    //don't set to same parent
    if (container->parent == parent)
        return;

    if (container->parent != NULL)
        e_container_remove_container(container->parent, container);

    e_container_add_container(parent, container);
}

void e_container_set_position(struct e_container* container, int lx, int ly)
{
    assert(container);

    wlr_scene_node_set_position(&container->tree->node, lx, ly);
    container->area.x = lx;
    container->area.y = ly;
}

void e_container_configure(struct e_container* container, int lx, int ly, int width, int height)
{
    assert(container);

    wlr_scene_node_set_position(&container->tree->node, lx, ly);
    container->area = (struct wlr_box){lx, ly, width, height};

    if (container->window != NULL)
        e_window_configure(container->window, 0, 0, width, height);
}

struct e_container* e_container_window_create(struct wlr_scene_tree* parent, struct e_window* window)
{
    assert(parent && window && window->scene_tree);

    struct e_container* container = e_container_create(parent, E_TILING_MODE_NONE);

    if (container == NULL)
    {
        e_log_error("Failed to allocate e_container");
        abort();
    }

    container->window = window;
    wlr_scene_node_reparent(&window->scene_tree->node, container->tree);

    return container;
}

bool e_container_contains_window(struct e_container* container)
{
    return (container->window != NULL);
}

//Arranges a containter's children (window or other containers) to fit within container's area
void e_container_arrange(struct e_container* container)
{
    assert(container);
    
    if (container->tiling_mode == E_TILING_MODE_NONE)
        return;

    float percentageStart = 0.0f;

    struct e_container* child_container;
    wl_list_for_each(child_container, &container->containers, link)
    {
        struct wlr_box child_area = {0, 0, container->area.width, container->area.height};

        switch (container->tiling_mode)
        {
            case E_TILING_MODE_HORIZONTAL:
                child_area.x = container->area.width * percentageStart;
                child_area.width *= child_container->percentage;
                break;
            case E_TILING_MODE_VERTICAL:
                child_area.y = container->area.height * percentageStart;
                child_area.height *= child_container->percentage;
                break;
            default:
                e_log_error("Unknown container tiling mode!");
                abort();
        }

        percentageStart += child_container->percentage;

        e_container_configure(child_container, child_area.x, child_area.y, child_area.width, child_area.height);
        e_container_arrange(child_container);
    }
}

void e_container_destroy(struct e_container* container)
{
    //TODO: possible memory leak here? container's destroy link is removed again after being removed

    assert(container);

    if (container->destroying)
    {
        e_log_error("already destroying container...");
        return;
    }

    if (container->window != NULL)
        e_log_info("destroying WINDOW container...");
    else if (container->parent == NULL)
        e_log_info("destroying ROOT container...");
    else
        e_log_info("destroying CONTAINER container...");
    
    container->destroying = true;

    if (container->tree != NULL)
        wlr_scene_node_destroy(&container->tree->node);
}