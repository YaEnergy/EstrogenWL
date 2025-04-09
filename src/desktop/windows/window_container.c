#include "desktop/windows/window_container.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>

#include "desktop/tree/container.h"
#include "desktop/tree/node.h"
#include "desktop/windows/window.h"

#include "input/seat.h"

#include "util/log.h"

//TODO: window container decoration
//TODO: destroy function might be called recursively?

static void e_window_container_set_parent(struct e_window_container* window_container, struct e_tree_container* parent)
{
    assert(window_container && parent);

    struct e_tree_container* previous_parent = window_container->base.parent;

    //parent container remains unchanged
    if (parent == previous_parent)
    {
        e_log_info("window container parent remains unchanged");
        return;
    }

    #if E_VERBOSE
    e_log_info("setting window container parent...");
    #endif

    //remove from & rearrange previous container
    if (previous_parent != NULL)
    {
        e_tree_container_remove_container(previous_parent, &window_container->base);
        e_tree_container_arrange(previous_parent);
    }

    e_tree_container_add_container(parent, &window_container->base);
    e_tree_container_arrange(parent);
}

static void e_window_container_tile(struct e_window_container* window_container)
{
    assert(window_container && window_container->view);

    //if not focused, find current TILED focused window's parent container
    // else if focused or if none, find previous TILED focused's parent container
    //if none, then output 0's root tiling container
    //add to container if found
    //else give up

    struct e_desktop* desktop = window_container->desktop;
    struct e_seat* seat = desktop->seat;

    struct wlr_surface* window_surface = window_container->view->surface;

    struct e_tree_container* parent_container = NULL;

    if (window_surface != NULL)
    {
        //if not focused, find current TILED focused window's parent container
        if (!e_seat_has_focus(seat, window_surface) && seat->focus_surface != NULL)
        {
            struct e_window* focused_window = e_window_from_surface(desktop, seat->focus_surface);

            if (focused_window != NULL && focused_window->container != NULL && focused_window->tiled && focused_window->container->base.parent != NULL)
                parent_container = focused_window->container->base.parent;
        }

        //if focused or none, find previous TILED focused's parent container
        if ((parent_container == NULL || e_seat_has_focus(seat, window_surface)) && seat->previous_focus_surface != NULL)
        {
            struct e_window* prev_focused_window = e_window_from_surface(desktop, seat->previous_focus_surface);

            if (prev_focused_window != NULL && prev_focused_window->container != NULL && prev_focused_window->tiled && prev_focused_window->container->base.parent != NULL)
                parent_container = prev_focused_window->container->base.parent;
        }
    }
    
    //if still none, then output 0's root tiling container
    if (parent_container == NULL)
    {
        //Set to output 0's container
        struct e_output* output = e_desktop_get_output(desktop, 0);

        if (output != NULL && output->root_tiling_container != NULL)
            parent_container = output->root_tiling_container;
    }

    if (parent_container == NULL)
    {
        e_log_error("Failed to set window container's parent to a tiled container");
        return;
    }
    
    e_window_container_set_parent(window_container, parent_container);
}

// Returns the window's first current output with a root floating container
// May return NULL.
static struct e_tree_container* e_window_first_output_floating_container(struct e_window* window)
{
    assert(window);

    if (window->surface == NULL)
        return NULL;

    if (wl_list_empty(&window->surface->current_outputs))
        return NULL;

    //loop over surface outputs until output is found with a root floating container
    struct wlr_surface_output* wlr_surface_output;
    wl_list_for_each(wlr_surface_output, &window->surface->current_outputs, link)
    {
        //attempt to acquire output from it
        if (wlr_surface_output->output != NULL && wlr_surface_output->output->data != NULL)
        {
            struct e_output* output = wlr_surface_output->output->data;

            if (output->root_floating_container != NULL)
                return output->root_floating_container;
        }
    }

    //none found
    return NULL;
}

static void e_window_container_float(struct e_window_container* window_container)
{
    assert(window_container && window_container->view);

    //find first of current outputs root floating container
    //if none, then output 0's root floating container
    //add to container if found
    //else give up

    struct e_window* window = window_container->view;

    struct wlr_surface* window_surface = window->surface;

    struct e_tree_container* parent_container = NULL;

    if (window_surface != NULL)
        parent_container = e_window_first_output_floating_container(window);

    if (parent_container == NULL)
    {
        //Set to output 0's container
        struct e_output* output = e_desktop_get_output(window_container->desktop, 0);

        if (output != NULL && output->root_floating_container != NULL)
            parent_container = output->root_floating_container;
    }

    if (parent_container == NULL)
    {
        e_log_error("Failed to set window container's parent to a floating container");
        return;
    }

    e_window_container_set_parent(window_container, parent_container);
}

static void e_window_container_view_set_tiled(struct wl_listener* listener, void* data)
{
    struct e_window_container* window_container = wl_container_of(listener, window_container, view_set_tiled);
    
    e_log_info("window container view set tiled");

    window_container->tiled = window_container->view->tiled;

    if (window_container->tiled)
        e_window_container_tile(window_container);
    else
        e_window_container_float(window_container);
}

static void e_window_container_view_set_title(struct wl_listener* listener, void* data)
{
    struct e_window_container* window_container = wl_container_of(listener, window_container, view_set_title);
    char* title = data;

    window_container->title = title;

    //TODO: when adding decoration, refresh title bar
}

static uint32_t e_container_configure_window_container(struct e_container* container, int lx, int ly, int width, int height)
{
    assert(container);

    return e_window_container_configure(container->data, lx, ly, width, height);
}

static void e_container_destroy_window_container(struct e_container* container)
{
    assert(container);

    e_window_container_destroy(container->data);
}

struct e_window_container* e_window_container_create(struct e_window* window)
{
    assert(window && window->tree->node.parent);

    struct e_window_container* window_container = calloc(1, sizeof(*window_container));

    if (window_container == NULL)
    {
        e_log_error("failed to alloc window_container");
        return NULL;
    }

    e_container_init(&window_container->base, window->desktop->pending, window_container);
    window_container->view = window;
    window_container->desktop = window->desktop;

    wlr_scene_node_reparent(&window->tree->node, window_container->base.tree);

    window_container->tiled = false;

    //implementation
    
    window_container->base.implementation.configure = e_container_configure_window_container;
    window_container->base.implementation.destroy = e_container_destroy_window_container;

    //events

    window_container->view_set_tiled.notify = e_window_container_view_set_tiled;
    wl_signal_add(&window_container->view->events.set_tiled, &window_container->view_set_tiled);

    window_container->view_set_title.notify = e_window_container_view_set_title;
    wl_signal_add(&window_container->view->events.set_title, &window_container->view_set_title);

    if (window->tiled)
        e_window_container_tile(window_container);
    else
        e_window_container_float(window_container);

    return window_container;
}

void e_window_container_set_position(struct e_window_container* window_container, int lx, int ly)
{
    e_container_set_position(&window_container->base, lx, ly);
}

uint32_t e_window_container_configure(struct e_window_container* window_container, int lx, int ly, int width, int height)
{
    assert(window_container);

    e_window_container_set_position(window_container, lx, ly);
    window_container->base.area.width = width;
    window_container->base.area.height = height;

    //TODO: configure decoration

    return e_window_configure(window_container->view, 0, 0, width, height);
}

uint32_t e_window_container_maximize(struct e_window_container* window_container)
{
    assert(window_container && window_container->base.parent);

    if (window_container->tiled)
        return 0;

    struct e_tree_container* parent = window_container->base.parent;

    return e_window_container_configure(window_container, parent->base.area.x, parent->base.area.y, parent->base.area.width, parent->base.area.height);
}

void e_window_container_set_tiled(struct e_window_container* window_container, bool tiled)
{
    assert(window_container);

    if (window_container->view == NULL)
    {
        e_log_error("window container has no window view!");
        return;
    }

    e_log_info("setting tiling mode of window to %d...", tiled);

    if (window_container->tiled == tiled)
        return;

    window_container->tiled = tiled;

    //Reparent to current focused container
    if (tiled)
        e_window_container_tile(window_container);
    else //floating
        e_window_container_float(window_container);

    e_window_set_tiled(window_container->view, tiled);
}

void e_window_container_send_close(struct e_window_container* window_container)
{
    assert(window_container);

    if (window_container->view != NULL)
        e_window_send_close(window_container->view);
}

void e_window_container_destroy(struct e_window_container* window_container)
{
    assert(window_container);

    #if E_VERBOSE
    e_log_info("window container destroy");
    #endif

    wl_list_remove(&window_container->view_set_tiled.link);
    wl_list_remove(&window_container->view_set_title.link);

    e_container_fini(&window_container->base);

    free(window_container);
}