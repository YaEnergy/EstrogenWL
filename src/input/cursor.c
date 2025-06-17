#include "input/cursor.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>

#include <wlr/util/box.h>
#include <wlr/util/edges.h>

#include "input/seat.h"

#include "desktop/desktop.h"
#include "desktop/views/view.h"
#include "desktop/tree/container.h"

#include "util/list.h"
#include "util/log.h"
#include "util/wl_macros.h"

//xcursor names: https://www.freedesktop.org/wiki/Specifications/cursor-spec/

// codes in /usr/include/linux/input-event-codes.h
#define E_POINTER_BUTTON_MIDDLE 0x112
#define E_POINTER_BUTTON_RIGHT 0x111
#define E_POINTER_BUTTON_LEFT 0x110

static void e_cursor_frame(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, frame);
    
    wlr_seat_pointer_notify_frame(cursor->seat->wlr_seat);
}

static void start_grab_resize_focused_view(struct e_cursor* cursor)
{
    assert(cursor);

    struct e_view* focused_view = e_desktop_focused_view(cursor->seat->desktop);

    if (focused_view == NULL)
        return;

    enum wlr_edges edges = WLR_EDGE_NONE;

    //get closest edges to cursor
    
    double mid_x = (double)focused_view->current.x + (double)focused_view->current.width / 2.0;
    
    if (cursor->wlr_cursor->x < mid_x)
        edges = WLR_EDGE_LEFT;
    else
        edges = WLR_EDGE_RIGHT;

    double mid_y = (double)focused_view->current.y + (double)focused_view->current.height / 2.0;

    if (cursor->wlr_cursor->y < mid_y)
        edges |= WLR_EDGE_TOP;
    else
        edges |= WLR_EDGE_BOTTOM;

    e_cursor_start_view_resize(cursor, focused_view, edges);
}

static void start_grab_move_focused_view(struct e_cursor* cursor)
{
    assert(cursor);

    struct e_view* focused_view = e_desktop_focused_view(cursor->seat->desktop);

    if (focused_view != NULL)
        e_cursor_start_view_move(cursor, focused_view);
}

//mouse button presses
static void e_cursor_button(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, button);
    struct wlr_pointer_button_event* event = data;

    bool handled = false;

    struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(cursor->seat->wlr_seat);

    //is ALT modifier is pressed on keyboard? (for both cases within here, cursor mode should be in default mode)
    if (keyboard != NULL && (wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_ALT) && cursor->mode == E_CURSOR_MODE_DEFAULT)
    {
        //right click is held, start resizing the focussed view
        if (event->button == E_POINTER_BUTTON_RIGHT && event->state == WL_POINTER_BUTTON_STATE_PRESSED)
        {
            start_grab_resize_focused_view(cursor);
            handled = true;
        }
        //middle click is held, start moving the focussed view
        else if (event->button == E_POINTER_BUTTON_MIDDLE && event->state == WL_POINTER_BUTTON_STATE_PRESSED)
        {
            start_grab_move_focused_view(cursor);
            handled = true;
        }
    }

    //reset cursor mode to default on button release
    if (event->state == WL_POINTER_BUTTON_STATE_RELEASED && cursor->mode != E_CURSOR_MODE_DEFAULT)
    {
        e_cursor_reset_mode(cursor);
        handled = true;
    }

    //send to clients if button hasn't been handled
    if (!handled)
        wlr_seat_pointer_notify_button(cursor->seat->wlr_seat, event->time_msec, event->button, event->state);
}

// Swaps 2 tiled view's parents and percentages.
static void swap_tiled_views(struct e_view* a, struct e_view* b)
{
    if (a == NULL)
    {
        e_log_error("swap_tiled_views: view A is NULL");
        return;
    }

    if (b == NULL)
    {
        e_log_error("swap_tiled_views: view B is NULL");
        return;
    }

    if (a->container.parent == NULL || !a->tiled)
    {
        e_log_error("swap_tiled_views: view A is not tiled!");
        return;
    }

    if (b->container.parent == NULL || !b->tiled)
    {
        e_log_error("swap_tiled_views: view B is not tiled!");
        return;
    }

    #if E_VERBOSE
    e_log_info("swapping tiled views");
    #endif

    //swap parents

    int index_a = e_list_find_index(&a->container.parent->children, &a->container);
    int index_b = e_list_find_index(&b->container.parent->children, &b->container);
    
    e_list_swap_outside(&a->container.parent->children, index_a, &b->container.parent->children, index_b);

    //above only swaps in the actual lists

    struct e_tree_container* tmp_parent = a->container.parent;
    a->container.parent = b->container.parent;
    b->container.parent = tmp_parent;

    //swap percentages

    float tmp_percentage = a->container.percentage;
    a->container.percentage = b->container.percentage;
    b->container.percentage = tmp_percentage;

    //rearrange tree containers

    e_tree_container_arrange(a->container.parent);

    //only rearrange once if both views have the same parent container
    if (b->container.parent != a->container.parent)
        e_tree_container_arrange(b->container.parent);
}

static void e_cursor_handle_mode_move(struct e_cursor* cursor)
{
    if (cursor->grab_view == NULL)
    {
        e_log_error("Cursor move mode: view grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "all-scroll");

    if (cursor->grab_view->tiled)
    {
        struct e_desktop* desktop = cursor->seat->desktop;
        
        //get current view under cursor
        struct e_view* cursor_view = e_view_at(&desktop->scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y);
    
        //if not the same tiled view as grabbed tiled view, swap them
        if (cursor_view != NULL && cursor_view->tiled && cursor_view != cursor->grab_view)
            swap_tiled_views(cursor_view, cursor->grab_view);
    }
    else //floating
    {
        float delta_x = cursor->wlr_cursor->x - cursor->grab_start_x;
        float delta_y = cursor->wlr_cursor->y - cursor->grab_start_y;
        e_view_set_position(cursor->grab_view, cursor->grab_start_vbox.x + delta_x, cursor->grab_start_vbox.y + delta_y);
    }
}

static bool edge_is_along_tiling_axis(enum wlr_edges edge, enum e_tiling_mode tiling_mode)
{
    if (tiling_mode == E_TILING_MODE_HORIZONTAL)
        return (edge & WLR_EDGE_LEFT || edge & WLR_EDGE_RIGHT);
    else if (tiling_mode == E_TILING_MODE_VERTICAL)
        return (edge & WLR_EDGE_TOP || edge & WLR_EDGE_BOTTOM);
    else //E_TILING_MODE_NONE
        return false; 
}

// Grow/shrink container's percentage to the given percentage along end or start by as much as it can.
// Returns if they're able to be resizd.
static bool resize_tiled_container(struct e_container* container, bool end, float percentage)
{
    if (container == NULL)
    {
        e_log_error("resize_tiled_container: container is NULL");
        return false;
    }

    if (container->parent == NULL || container->parent->tiling_mode == E_TILING_MODE_NONE)
    {
        e_log_error("resize_tiled_container: container is not tiled!");
        return false;
    }

    struct e_container* affected_container = NULL;

    if (end)
        affected_container = e_container_next_sibling(container);
    else
        affected_container = e_container_prev_sibling(container);

    if (affected_container == NULL)
    {
        e_log_error("resize_tiled_container: no other container is affected by resize, can't resize");
        return false;
    }

    float total_percentage = container->percentage + affected_container->percentage;

    //TODO: should limit by view constraints
    //TODO: 5% is kind of arbitrary
    //TODO: respect min size hint if possible

    //can't resize without breaking limits
    if (total_percentage < 0.1f)
        return false;

    //limit size of affected container, keep min 5%
    if (total_percentage - percentage < 0.05f)
        percentage = total_percentage - 0.05f;
    //limit size of main container, keep min 5%
    else if (percentage < 0.05f)
        percentage = 0.05f;

    container->percentage = percentage;
    affected_container->percentage  = total_percentage - percentage;
    
    return true;
}

static void e_cursor_resize_tiled(struct e_cursor* cursor)
{
    if (cursor == NULL)
    {
        e_log_error("e_cursor_resize_tiling: cursor is NULL");
        return;
    }

    if (cursor->grab_view == NULL)
    {
        e_log_error("e_cursor_resize_tiling: view grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    if (cursor->grab_view->container.parent == NULL || !cursor->grab_view->tiled)
    {
        e_log_error("e_cursor_resize_tiling: view grabbed by cursor is not tiled");
        e_cursor_reset_mode(cursor);
        return;
    }

    //TODO: implement e_cursor_resize_tiled
    //TODO: clean up & refactor
    //TODO: use starting container percentage

    //if grabbed edge is along the direction of the grabbed view's parent container, update grabbed view's container percentage (not parent)
    //else update grabbed view's PARENT container's percentage
    
    struct e_container* container_resize = NULL;

    if (edge_is_along_tiling_axis(cursor->grab_edges, cursor->grab_view->container.parent->tiling_mode))
        container_resize = &cursor->grab_view->container;
    else
        container_resize = &cursor->grab_view->container.parent->base;

    if (container_resize->parent == NULL)
    {
        e_log_error("e_cursor_resize_tiling: container to resize has no parent!");
        e_cursor_reset_mode(cursor);
        return;
    }

    enum e_tiling_mode tiling_axis = container_resize->parent->tiling_mode;

    int size = (tiling_axis == E_TILING_MODE_HORIZONTAL) ? container_resize->parent->base.area.width : container_resize->parent->base.area.height;
    double cursor_pos = (tiling_axis == E_TILING_MODE_HORIZONTAL) ? cursor->wlr_cursor->x : cursor->wlr_cursor->y;

    double grab_start_pos = (tiling_axis == E_TILING_MODE_HORIZONTAL) ? cursor->grab_start_x : cursor->grab_start_y;

    e_log_info("new: %g, start grab: %g", cursor_pos, grab_start_pos);

    //calc abs percentage moved from start pos
    float delta_percentage = (cursor_pos - grab_start_pos) / (float)size;

    //are we growing container along its end? right edge in horizontal tiling, bottom edge in vertical tiling
    bool end = (cursor->grab_edges & WLR_EDGE_RIGHT && tiling_axis == E_TILING_MODE_HORIZONTAL) || (cursor->grab_edges & WLR_EDGE_BOTTOM && tiling_axis == E_TILING_MODE_VERTICAL);
    
    //if resizing from the beginning, going left/up should grow container instead of shrinking
    if (!end)
        delta_percentage = -delta_percentage;

    e_log_info("delta percentage: %g", delta_percentage);

    resize_tiled_container(container_resize, end, cursor->grab_start_tile_percentage + delta_percentage);
    
    e_log_info("new percentage %g", container_resize->percentage);

    e_tree_container_arrange(container_resize->parent);
}

static void e_cursor_resize_floating(struct e_cursor* cursor)
{
    if (cursor == NULL)
    {
        e_log_error("e_cursor_resize_floating: cursor is NULL");
        return;
    }

    if (cursor->grab_view == NULL)
    {
        e_log_error("e_cursor_resize_floating: view grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    struct e_view_size_hints size_hints = e_view_get_size_hints(cursor->grab_view);

    int left = cursor->grab_start_vbox.x;
    int right = cursor->grab_start_vbox.x + cursor->grab_start_vbox.width;
    int top = cursor->grab_start_vbox.y;
    int bottom = cursor->grab_start_vbox.y + cursor->grab_start_vbox.height;

    //delta x & delta y since start grab
    int grow_x = cursor->wlr_cursor->x - cursor->grab_start_x;
    int grow_y = cursor->wlr_cursor->y - cursor->grab_start_y;

    //expand grabbed edges, while not letting them overlap the opposite edge, min 1 pixel

    if (cursor->grab_edges & WLR_EDGE_LEFT)
    {
        left += grow_x;

        //respect size hints

        if (right - left < size_hints.min_width && size_hints.min_width > 0)
            left = right - size_hints.min_width;

        if (right - left > size_hints.max_width && size_hints.max_width > 0)
            left = right - size_hints.max_width;

        // don't let box overlap itself

        if (left > right - 1)
            left = right - 1;
    }
    else if (cursor->grab_edges & WLR_EDGE_RIGHT)
    {
        right += grow_x;

        //respect size hints

        if (right - left < size_hints.min_width && size_hints.min_width > 0)
            right = left + size_hints.min_width;

        if (right - left > size_hints.max_width && size_hints.max_width > 0)
            right = left + size_hints.max_width;

        // don't let box overlap itself

        if (right < left + 1)
            right = left + 1;
    }

    if (cursor->grab_edges & WLR_EDGE_TOP)
    {
        top += grow_y;

        //respect size hints

        if (bottom - top < size_hints.min_height && size_hints.min_height > 0)
            top = bottom - size_hints.min_height;

        if (bottom - top > size_hints.max_height && size_hints.max_height > 0)
            top = bottom - size_hints.max_height;

        // don't let box overlap itself

        if (top > bottom - 1)
            top = bottom - 1;
    }
    else if (cursor->grab_edges & WLR_EDGE_BOTTOM)
    {
        bottom += grow_y;

        //respect size hints

        if (bottom - top < size_hints.min_height && size_hints.min_height > 0)
            bottom = top + size_hints.min_height;

        if (bottom - top > size_hints.max_height && size_hints.max_height > 0)
            bottom = top + size_hints.max_height;

        // don't let box overlap itself

        if (bottom < top + 1)
            bottom = top + 1;
    }

    e_view_configure(cursor->grab_view, left, top, right - left, bottom - top);
}

static void e_cursor_handle_mode_resize(struct e_cursor* cursor)
{   
    if (cursor->grab_view == NULL)
    {
        e_log_error("Cursor resize mode: view grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    //TODO: most likely due to some imprecision of some kind (idk), there seems to be movement on the right edge and bottom edge when resizing their opposite edges
    //TODO: wait for view to finish committing before resizing again
    //above has been fixed for toplevel views, but xwayland views seem to still have some issues.

    if (cursor->grab_view->tiled)
        e_cursor_resize_tiled(cursor);
    else //floating
        e_cursor_resize_floating(cursor);
}

static void e_cursor_handle_motion(struct e_cursor* cursor, uint32_t time_msec)
{
    switch (cursor->mode)
    {
        case E_CURSOR_MODE_MOVE:
            e_cursor_handle_mode_move(cursor);
            return;
        case E_CURSOR_MODE_RESIZE:
            e_cursor_handle_mode_resize(cursor);
            return;
        default:
            break;
    }

    struct e_desktop* desktop = cursor->seat->desktop;
    struct e_seat* seat = cursor->seat;

    double sx, sy;
    struct wlr_scene_surface* hover_surface = e_desktop_scene_surface_at(&desktop->scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &sx, &sy);

    e_cursor_set_focus_hover(cursor);

    if (hover_surface != NULL)
        wlr_seat_pointer_notify_motion(seat->wlr_seat, time_msec, sx, sy);
}

static void e_cursor_motion(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, motion);
    struct wlr_pointer_motion_event* event = data;

    //move & send to clients
    wlr_cursor_move(cursor->wlr_cursor, &event->pointer->base, event->delta_x, event->delta_y);

    e_cursor_handle_motion(cursor, event->time_msec);
}

static void e_cursor_motion_absolute(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, motion_absolute);
    struct wlr_pointer_motion_absolute_event* event = data;

    //move & send to clients
    wlr_cursor_warp_absolute(cursor->wlr_cursor, &event->pointer->base, event->x, event->y);
    
    e_cursor_handle_motion(cursor, event->time_msec);
}

//scroll event
static void e_cursor_axis(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, axis);
    struct wlr_pointer_axis_event* event = data;
    
    //send to clients
    wlr_seat_pointer_notify_axis(cursor->seat->wlr_seat, event->time_msec, event->orientation, event->delta, event->delta_discrete, event->source, event->relative_direction);
}

// Grabbed view was unmapped, let go.
static void e_cursor_grab_view_unmap(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, grab_view_unmap);

    e_log_info("grabbed view was unmapped");

    e_cursor_reset_mode(cursor);
}

struct e_cursor* e_cursor_create(struct e_seat* seat, struct wlr_output_layout* output_layout)
{
    assert(seat && output_layout);

    struct e_cursor* cursor = calloc(1, sizeof(*cursor));

    if (cursor == NULL)
    {
        e_log_error("failed to allocate cursor");
        return NULL;
    }

    cursor->seat = seat;
    cursor->mode = E_CURSOR_MODE_DEFAULT;

    cursor->wlr_cursor = wlr_cursor_create();

    cursor->grab_view = NULL;

    //boundaries and movement semantics of cursor
    wlr_cursor_attach_output_layout(cursor->wlr_cursor, output_layout);

    //load xcursor theme
    cursor->xcursor_manager = wlr_xcursor_manager_create(NULL, 24);
    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "default");

    // events

    SIGNAL_CONNECT(cursor->wlr_cursor->events.frame, cursor->frame, e_cursor_frame);
    SIGNAL_CONNECT(cursor->wlr_cursor->events.button, cursor->button, e_cursor_button);
    SIGNAL_CONNECT(cursor->wlr_cursor->events.motion, cursor->motion, e_cursor_motion);
    SIGNAL_CONNECT(cursor->wlr_cursor->events.motion_absolute, cursor->motion_absolute, e_cursor_motion_absolute);
    SIGNAL_CONNECT(cursor->wlr_cursor->events.axis, cursor->axis, e_cursor_axis);

    return cursor;
}

void e_cursor_set_mode(struct e_cursor* cursor, enum e_cursor_mode mode)
{
    e_log_info("set mode");
    cursor->mode = mode;
}

void e_cursor_reset_mode(struct e_cursor* cursor)
{
    e_log_info("reset mode");

    cursor->mode = E_CURSOR_MODE_DEFAULT;
    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "default");

    //let go of view
    if (cursor->grab_view != NULL)
    {
        cursor->grab_view = NULL;
        SIGNAL_DISCONNECT(cursor->grab_view_unmap);
    }
}

// Start grabbing a view under the given mode. (should be RESIZE or MOVE)
static void e_cursor_start_grab_view_mode(struct e_cursor* cursor, struct e_view* view, enum e_cursor_mode mode)
{
    assert(cursor);

    if (view == NULL)
        return;

    e_cursor_set_mode(cursor, mode);

    cursor->grab_view = view;
    cursor->grab_start_vbox = view->current;

    cursor->grab_start_x = cursor->wlr_cursor->x;
    cursor->grab_start_y = cursor->wlr_cursor->y;

    cursor->grab_start_tile_percentage = view->container.percentage;

    SIGNAL_CONNECT(view->surface->events.unmap, cursor->grab_view_unmap, e_cursor_grab_view_unmap);

    e_log_info("Grabbed view");

    #if E_VERBOSE
    e_log_info("Cursor grab position: XY(%f, %f)", cursor->grab_start_x, cursor->grab_start_y);
    e_log_info("View current rect: XY(%i, %i); WH(%i, %i)", view->current.x, view->current.y, view->current.width, view->current.height);
    #endif
}

static void e_cursor_set_xcursor_resize(struct e_cursor* cursor, enum wlr_edges edges)
{
    int i = 0;
    char direction[5];
    
    if (edges & WLR_EDGE_TOP)
    {
        direction[i] = 'n';
        i++;
    }
        
    if (edges & WLR_EDGE_BOTTOM)
    {
        direction[i] = 's';
        i++;
    }

    if (edges & WLR_EDGE_RIGHT)
    {
        direction[i] = 'e';
        i++;
    }
        
    if (edges & WLR_EDGE_LEFT)
    {
        direction[i] = 'w';
        i++;
    }
    
    //no edges can be resized or more than 2 directions
    if (i < 1 || i > 2)
    {
        wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "not-allowed");
        return;
    }

    direction[i] = '\0'; //null terminator

    const int MAX_NAME_LENGTH = 11; // 2 direction chars + "-resize" + '\0'
    char resize_cursor_name[MAX_NAME_LENGTH];

    int length = snprintf(resize_cursor_name, MAX_NAME_LENGTH, "%s-resize", direction);

    if (length > MAX_NAME_LENGTH)
        return;

    //Resize cursor name is copied.
    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, resize_cursor_name);
}

void e_cursor_start_view_resize(struct e_cursor* cursor, struct e_view* view, enum wlr_edges edges)
{
    if (view == NULL)
        return;
    
    if (view->fullscreen)
    {
        e_log_error("e_cursor_start_view_resize: can't resize views in fullscreen mode!");
        return;
    }

    cursor->grab_edges = edges;

    e_cursor_start_grab_view_mode(cursor, view, E_CURSOR_MODE_RESIZE);

    e_cursor_set_xcursor_resize(cursor, edges);
}

void e_cursor_start_view_move(struct e_cursor* cursor, struct e_view* view)
{
    assert(cursor);

    if (view == NULL)
        return;

    if (view->fullscreen)
    {
        e_log_error("e_cursor_start_view_move: can't move views in fullscreen mode!");
        return;
    }

    e_cursor_start_grab_view_mode(cursor, view, E_CURSOR_MODE_MOVE);
}

// Sets seat focus to whatever surface is under cursor.
// If nothing is under cursor, doesn't change seat focus.
void e_cursor_set_focus_hover(struct e_cursor* cursor)
{
    //only update focus in default mode
    if (cursor->mode != E_CURSOR_MODE_DEFAULT)
        return;

    struct e_desktop* desktop = cursor->seat->desktop;
    struct e_seat* seat = cursor->seat;

    double sx, sy;
    struct wlr_scene_surface* hover_surface = e_desktop_scene_surface_at(&desktop->scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &sx, &sy);
    struct e_view* view = (hover_surface == NULL) ? NULL : e_view_try_from_node_ancestors(&hover_surface->buffer->node);

    if (hover_surface != NULL)
    {
        wlr_seat_pointer_notify_enter(seat->wlr_seat, hover_surface->surface, sx, sy); //is only sent once

        //sloppy focus
        e_desktop_focus_surface(desktop, hover_surface->surface);
    }
    else 
    {
        wlr_seat_pointer_notify_clear_focus(seat->wlr_seat);
    }

    //display default cursor when not hovering any VIEWS (not just any surface)
    if (view == NULL)
        wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "default");
}

void e_cursor_destroy(struct e_cursor* cursor)
{
    assert(cursor);

    //let go of view
    if (cursor->grab_view != NULL)
        e_cursor_reset_mode(cursor);

    SIGNAL_DISCONNECT(cursor->frame);
    SIGNAL_DISCONNECT(cursor->button);
    SIGNAL_DISCONNECT(cursor->motion);
    SIGNAL_DISCONNECT(cursor->motion_absolute);
    SIGNAL_DISCONNECT(cursor->axis);

    wlr_xcursor_manager_destroy(cursor->xcursor_manager);
    wlr_cursor_destroy(cursor->wlr_cursor);

    free(cursor);
}
