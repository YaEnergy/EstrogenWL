#include "desktop/views/window.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_scene.h>

#include "desktop/tree/container.h"
#include "desktop/tree/node.h"
#include "desktop/views/view.h"

#include "desktop/desktop.h"

#include "input/seat.h"

#include "util/log.h"

//temporary value for border thickness in pixels
#define BORDER_THICKNESS 3

static void e_window_set_parent(struct e_window* window, struct e_tree_container* parent)
{
    assert(window && parent);

    struct e_tree_container* previous_parent = window->base.parent;

    //parent container remains unchanged
    if (parent == previous_parent)
    {
        e_log_info("window parent remains unchanged");
        return;
    }

    #if E_VERBOSE
    e_log_info("setting window parent...");
    #endif

    //remove from & rearrange previous container
    if (previous_parent != NULL)
    {
        e_tree_container_remove_container(previous_parent, &window->base);
        e_tree_container_arrange(previous_parent);
    }

    e_tree_container_add_container(parent, &window->base);
    e_tree_container_arrange(parent);
}

static void e_window_tile(struct e_window* window)
{
    assert(window && window->view);

    #if E_VERBOSE
    e_log_info("window tile container");
    #endif

    //if not focused, find current TILED focused view's parent container
    // else if focused or if none, find previous TILED focused's parent container
    //if none, then output 0's root tiling container
    //add to container if found
    //else give up

    struct e_desktop* desktop = window->desktop;
    struct e_seat* seat = desktop->seat;

    struct wlr_surface* view_surface = window->view->surface;

    struct e_tree_container* parent_container = NULL;

    if (view_surface != NULL)
    {
        //if not focused, find current TILED focused windows's parent container
        if (!e_seat_has_focus(seat, view_surface) && seat->focus_surface != NULL)
        {
            struct e_window* focused_window = e_seat_focused_window(seat);

            if (focused_window != NULL && focused_window->tiled && focused_window->base.parent != NULL)
                parent_container = focused_window->base.parent;
        }

        //if focused or none, find previous TILED focused's parent container
        if ((parent_container == NULL || e_seat_has_focus(seat, view_surface)) && seat->previous_focus_surface != NULL)
        {
            struct e_window* prev_focused_window = e_seat_prev_focused_window(seat);

            if (prev_focused_window != NULL && prev_focused_window->tiled && prev_focused_window->base.parent != NULL)
                parent_container = prev_focused_window->base.parent;
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
        e_log_error("Failed to set window's parent to a tiled container");
        return;
    }
    
    e_window_set_parent(window, parent_container);
}

// Returns the view's first current output with a root floating container
// May return NULL.
static struct e_tree_container* e_view_first_output_floating_container(struct e_view* view)
{
    assert(view);

    if (view->surface == NULL)
        return NULL;

    if (wl_list_empty(&view->surface->current_outputs))
        return NULL;

    //loop over surface outputs until output is found with a root floating container
    struct wlr_surface_output* wlr_surface_output;
    wl_list_for_each(wlr_surface_output, &view->surface->current_outputs, link)
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

static void e_window_float(struct e_window* window)
{
    assert(window && window->view);

    #if E_VERBOSE
    e_log_info("window float container");
    #endif

    //find first of current outputs root floating container
    //if none, then output 0's root floating container
    //add to container if found
    //else give up

    struct e_view* view = window->view;

    struct wlr_surface* view_surface = view->surface;

    struct e_tree_container* parent_container = NULL;

    if (view_surface != NULL)
        parent_container = e_view_first_output_floating_container(view);

    if (parent_container == NULL)
    {
        //Set to output 0's container
        struct e_output* output = e_desktop_get_output(window->desktop, 0);

        if (output != NULL && output->root_floating_container != NULL)
            parent_container = output->root_floating_container;
    }

    if (parent_container == NULL)
    {
        e_log_error("Failed to set window's parent to a floating container");
        return;
    }

    e_window_set_parent(window, parent_container);
}

static void e_window_view_set_title(struct wl_listener* listener, void* data)
{
    struct e_window* window = wl_container_of(listener, window, view_set_title);
    char* title = data;

    window->title = title;

    //TODO: when adding decoration, refresh title bar
}

static uint32_t e_container_configure_window(struct e_container* container, int lx, int ly, int width, int height)
{
    assert(container);

    return e_window_configure(container->data, lx, ly, width, height);
}

static void e_container_destroy_window(struct e_container* container)
{
    assert(container);

    e_window_destroy(container->data);
}

static void e_window_init_view(struct e_window* window, struct e_view* view)
{
    assert(window && view);

    window->view = view;

    window->title = view->title;

    int width = view->current.width + BORDER_THICKNESS * 2;
    int height = view->current.height + BORDER_THICKNESS * 2;

    float color[4];
    color[0] = 1.0;
    color[1] = 1.0;
    color[2] = 1.0;
    color[3] = 1.0;

    window->background = wlr_scene_rect_create(window->base.tree, width, height, color);
    wlr_scene_node_set_position(&window->background->node, 0, 0);

    window->base.area.width = width;
    window->base.area.height = height;

    wlr_scene_node_reparent(&view->tree->node, window->base.tree);

    // events

    window->view_set_title.notify = e_window_view_set_title;
    wl_signal_add(&window->view->events.set_title, &window->view_set_title);

    // tile or float window

    window->tiled = view->tiled;

    if (window->tiled)
        e_window_tile(window);
    else
        e_window_float(window);
}

struct e_window* e_window_create(struct e_view* view)
{
    assert(view && view->tree->node.parent);

    struct e_window* window = calloc(1, sizeof(*window));

    if (window == NULL)
    {
        e_log_error("failed to alloc window");
        return NULL;
    }

    e_container_init(&window->base, view->desktop->pending, E_CONTAINER_WINDOW, window);
    window->desktop = view->desktop;

    //implementation
    
    window->base.implementation.configure = e_container_configure_window;
    window->base.implementation.destroy = e_container_destroy_window;

    e_window_init_view(window, view);

    return window;
}

void e_window_set_position(struct e_window* window, int lx, int ly)
{
    e_container_set_position(&window->base, lx, ly);
}

uint32_t e_window_configure(struct e_window* window, int lx, int ly, int width, int height)
{
    assert(window);

    #if E_VERBOSE
    e_log_info("window configure");
    #endif

    //Min size: border + 1 pixel
    if (width < BORDER_THICKNESS * 2 + 1)
        width = BORDER_THICKNESS * 2 + 1;

    if (height < BORDER_THICKNESS * 2 + 1)
        height = BORDER_THICKNESS * 2 + 1;

    //Container
    e_window_set_position(window, lx, ly);
    window->base.area.width = width;
    window->base.area.height = height;
    wlr_scene_rect_set_size(window->background, width, height);

    return e_view_configure(window->view, BORDER_THICKNESS, BORDER_THICKNESS, width - BORDER_THICKNESS * 2, height - BORDER_THICKNESS * 2);
}

uint32_t e_window_maximize(struct e_window* window)
{
    assert(window && window->base.parent);

    if (window->tiled)
        return 0;

    struct e_tree_container* parent = window->base.parent;

    return e_window_configure(window, 0, 0, parent->base.area.width, parent->base.area.height);
}

void e_window_set_tiled(struct e_window* window, bool tiled)
{
    assert(window);

    if (window->view == NULL)
    {
        e_log_error("window has no view!");
        return;
    }

    e_log_info("setting tiling mode of window to %d...", tiled);

    window->tiled = tiled;

    //Reparent to current focused container
    if (tiled)
        e_window_tile(window);
    else //floating
        e_window_float(window);

    e_view_set_tiled(window->view, tiled);
}

// Returns NULL on fail.
struct e_window* e_window_try_from_node(struct wlr_scene_node* node)
{
    if (node == NULL)
        return NULL;

    //data is either NULL or e_node_desc
    if (node->data == NULL)
        return NULL;

    struct e_node_desc* node_desc = node->data;

    if (node_desc->type != E_NODE_DESC_CONTAINER)
        return NULL;

    struct e_container* container = node_desc->data;

    if (container->type == E_CONTAINER_WINDOW)
        return container->data;
    else
        return NULL;
}

// Returns NULL on fail.
struct e_window* e_window_try_from_node_ancestors(struct wlr_scene_node* node)
{
    if (node == NULL)
        return NULL;

    struct e_window* window = e_window_try_from_node(node);

    if (window != NULL)
        return window;

    //keep going upwards in the tree until we find a window (in which case we return it), or reach the root of the tree (no parent)
    while (node->parent != NULL)
    {
        //go to parent node
        node = &node->parent->node;

        struct e_window* window = e_window_try_from_node(node);

        if (window != NULL)
            return window;
    }

    return NULL;
}

// Searches for a window at the specified layout coords in the given scene graph.
// Returns NULL on fail.
struct e_window* e_window_at(struct wlr_scene_node* node, double lx, double ly)
{
    assert(node);

    double nx, ny = 0.0;
    struct wlr_scene_node* node_found = wlr_scene_node_at(node, lx, ly, &nx, &ny);

    if (node_found == NULL)
        return NULL;

    return e_window_try_from_node_ancestors(node_found);
}

void e_window_send_close(struct e_window* window)
{
    assert(window);

    if (window->view != NULL)
        e_view_send_close(window->view);
}

void e_window_destroy(struct e_window* window)
{
    assert(window);

    #if E_VERBOSE
    e_log_info("window destroy");
    #endif

    wl_list_remove(&window->view_set_title.link);

    //keep content tree, it's owned by view
    if (window->view->tree != NULL)
        wlr_scene_node_reparent(&window->view->tree->node, window->desktop->pending);

    e_container_fini(&window->base);

    free(window);
}