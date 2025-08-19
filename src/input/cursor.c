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

#include "server.h"

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

//TODO: e_container instead of e_view_container
static void start_grab_resize_focused_view_container(struct e_cursor* cursor)
{
    assert(cursor);

    struct e_view_container* focused_view_container = e_desktop_focused_view_container(cursor->seat->server);
    
    if (focused_view_container == NULL)
        return;

    struct e_container* focused_container = &focused_view_container->base;

    enum wlr_edges edges = WLR_EDGE_NONE;

    //get closest edges to cursor

    double mid_x = (double)focused_container->area.x + (double)focused_container->area.width / 2.0;
    
    if (cursor->wlr_cursor->x < mid_x)
        edges = WLR_EDGE_LEFT;
    else
        edges = WLR_EDGE_RIGHT;

    double mid_y = (double)focused_container->area.y + (double)focused_container->area.height / 2.0;

    if (cursor->wlr_cursor->y < mid_y)
        edges |= WLR_EDGE_TOP;
    else
        edges |= WLR_EDGE_BOTTOM;

    e_cursor_start_container_resize(cursor, focused_container, edges);
}

//TODO: e_container instead of e_view_container
static void start_grab_move_focused_view_container(struct e_cursor* cursor)
{
    assert(cursor);

    struct e_view_container* focused_view_container = e_desktop_focused_view_container(cursor->seat->server);

    if (focused_view_container != NULL)
        e_cursor_start_container_move(cursor, &focused_view_container->base);
}

//mouse button presses
static void e_cursor_button(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, button);
    struct wlr_pointer_button_event* event = data;

    bool handled = false;

    struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(cursor->seat->wlr_seat);

    //is ALT modifier is pressed on keyboard? (for both cases within here, cursor mode should be in default mode)
    if (keyboard != NULL && (wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_LOGO) && cursor->mode == E_CURSOR_MODE_DEFAULT)
    {
        //right click is held, start resizing the focused container
        if (event->button == E_POINTER_BUTTON_RIGHT && event->state == WL_POINTER_BUTTON_STATE_PRESSED)
        {
            start_grab_resize_focused_view_container(cursor);
            handled = true;
        }
        //left click is held, start moving the focused container
        else if (event->button == E_POINTER_BUTTON_LEFT && event->state == WL_POINTER_BUTTON_STATE_PRESSED)
        {
            start_grab_move_focused_view_container(cursor);
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

// TODO: move to tree container?
// Swaps 2 tiled container's parents and percentages.
static void swap_tiled_containers(struct e_container* a, struct e_container* b)
{
    if (a == NULL)
    {
        e_log_error("swap_tiled_containers: container A is NULL");
        return;
    }

    if (b == NULL)
    {
        e_log_error("swap_tiled_containers: container B is NULL");
        return;
    }

    if (!e_container_is_tiled(a))
    {
        e_log_error("swap_tiled_containers: container A is not tiled!");
        return;
    }

    if (!e_container_is_tiled(b))
    {
        e_log_error("swap_tiled_containers: container B is not tiled!");
        return;
    }

    #if E_VERBOSE
    e_log_info("swapping tiled containers");
    #endif

    //swap parents

    int index_a = e_list_find_index(&a->parent->children, a);
    int index_b = e_list_find_index(&b->parent->children, b);
    
    e_list_swap_outside(&a->parent->children, index_a, &b->parent->children, index_b);

    //above only swaps in the actual lists

    struct e_tree_container* tmp_parent = a->parent;
    a->parent = b->parent;
    b->parent = tmp_parent;

    //swap percentages

    float tmp_percentage = a->percentage;
    a->percentage = b->percentage;
    b->percentage = tmp_percentage;

    //rearrange tree containers

    e_container_arrange(&a->parent->base);

    //only rearrange once if both views have the same parent container
    if (b->parent != a->parent)
        e_container_arrange(&b->parent->base);
}

static void e_cursor_handle_mode_move(struct e_cursor* cursor)
{
    assert(cursor);

    if (cursor->grab_container == NULL)
    {
        e_log_error("Cursor move mode: container grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "all-scroll");

    if (e_container_is_tiled(cursor->grab_container))
    {
        //TODO: tiled container interactive reparenting
        struct e_server* server = cursor->seat->server;

        struct e_container* hovered_container = e_desktop_hovered_container(server);
    
        //if not the same tiled container as grabbed tiled container, swap them
        if (hovered_container != NULL && e_container_is_tiled(hovered_container) && hovered_container != cursor->grab_container)
            swap_tiled_containers(hovered_container, cursor->grab_container);
    }
    else //floating
    {
        float delta_x = cursor->wlr_cursor->x - cursor->grab_start_x;
        float delta_y = cursor->wlr_cursor->y - cursor->grab_start_y;

        cursor->grab_container->area = (struct wlr_box){cursor->grab_start_cbox.x + delta_x, cursor->grab_start_cbox.y + delta_y, cursor->grab_start_cbox.width, cursor->grab_start_cbox.height};
        e_container_arrange(cursor->grab_container);
    }
}

// Checks whether the given edges include an edge thats intersects the given tiling mode axis.
// edges is bitmask of enum wlr_edges.
static bool edges_has_intersection(uint32_t edges, enum e_tiling_mode tiling_mode)
{
    switch (tiling_mode)
    {
        case E_TILING_MODE_HORIZONTAL:
            return (edges & WLR_EDGE_LEFT || edges & WLR_EDGE_RIGHT);
        case E_TILING_MODE_VERTICAL:
            return (edges & WLR_EDGE_TOP || edges & WLR_EDGE_BOTTOM);
        default:
            return false;
    }
}

static void e_cursor_resize_tiled(struct e_cursor* cursor)
{
    assert(cursor);

    //if grabbed edge is along the direction of the grabbed container's parent, update grabbed container percentage (not parent)
    //else update grabbed container's parent percentage
    
    struct e_container* container_resize = NULL;

    if (edges_has_intersection(cursor->grab_edges, cursor->grab_container->parent->tiling_mode))
        container_resize = cursor->grab_container;
    else
        container_resize = &cursor->grab_container->parent->base;

    if (container_resize->parent == NULL)
    {
        e_log_error("e_cursor_resize_tiled: container to resize has no parent!");
        e_cursor_reset_mode(cursor);
        return;
    }

    enum e_tiling_mode tiling_axis = container_resize->parent->tiling_mode;

    int size = (tiling_axis == E_TILING_MODE_HORIZONTAL) ? container_resize->parent->base.area.width : container_resize->parent->base.area.height;
    double cursor_pos = (tiling_axis == E_TILING_MODE_HORIZONTAL) ? cursor->wlr_cursor->x : cursor->wlr_cursor->y;

    double grab_start_pos = (tiling_axis == E_TILING_MODE_HORIZONTAL) ? cursor->grab_start_x : cursor->grab_start_y;

    //calc percentage moved from start pos
    float delta_percentage = (cursor_pos - grab_start_pos) / (float)size;

    //are we resizing container along its end? right edge in horizontal tiling, bottom edge in vertical tiling
    bool end = (cursor->grab_edges & WLR_EDGE_RIGHT && tiling_axis == E_TILING_MODE_HORIZONTAL) || (cursor->grab_edges & WLR_EDGE_BOTTOM && tiling_axis == E_TILING_MODE_VERTICAL);
    
    //if resizing from the beginning, going left/up should grow container instead of shrinking
    if (!end)
        delta_percentage = -delta_percentage;

    struct e_container* affected = end ? e_container_next_sibling(container_resize) : e_container_prev_sibling(container_resize);

    if (affected != NULL)
    {
        e_container_resize_tiled(container_resize, affected, cursor->grab_start_tile_percentage + delta_percentage);
        
        //rearrange parent
        e_container_arrange(&container_resize->parent->base);
    }
    else 
    {
        e_log_error("e_cursor_resize_tiled: no container sibling can be affected! can't resize");
        e_cursor_reset_mode(cursor);
    }
}

static void e_cursor_resize_floating(struct e_cursor* cursor)
{
    assert(cursor);

    if (cursor == NULL)
    {
        e_log_error("e_cursor_resize_floating: cursor is NULL");
        return;
    }

    if (cursor->grab_container == NULL)
    {
        e_log_error("e_cursor_resize_floating: container grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    struct e_container_size_hints size_hints = e_container_get_size_hints(cursor->grab_container);

    //TODO: account for size increment hints (width_inc & height_inc)

    int left = cursor->grab_start_cbox.x;
    int right = cursor->grab_start_cbox.x + cursor->grab_start_cbox.width;
    int top = cursor->grab_start_cbox.y;
    int bottom = cursor->grab_start_cbox.y + cursor->grab_start_cbox.height;

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

        //don't let box overlap itself

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

        //don't let box overlap itself

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

        //don't let box overlap itself

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

    cursor->grab_container->area = (struct wlr_box){left, top, right - left, bottom - top};
    e_container_arrange(cursor->grab_container);
}

static void e_cursor_handle_mode_resize(struct e_cursor* cursor)
{
    assert(cursor);

    if (cursor->grab_container == NULL)
    {
        e_log_error("Cursor resize mode: container grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    //TODO: wait for view to finish committing (resize) before applying pending position

    if (e_container_is_tiled(cursor->grab_container))
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

    struct e_server* server = cursor->seat->server;
    struct e_seat* seat = cursor->seat;

    double sx, sy;
    struct wlr_scene_surface* hover_surface = e_desktop_scene_surface_at(&server->scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &sx, &sy);

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

// Grabbed container was destroyed, let go.
static void e_cursor_handle_grab_container_destroy(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, grab_container_destroy);

    e_log_info("grabbed container was destroyed");

    e_cursor_reset_mode(cursor);
}

static const char* get_xcursor_theme_name(void)
{
    const char* name = getenv("XCURSOR_THEME");
    
    if (name == NULL)
        name = "default";

    return name;
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

    cursor->grab_container = NULL;

    //boundaries and movement semantics of cursor
    wlr_cursor_attach_output_layout(cursor->wlr_cursor, output_layout);

    //load xcursor theme
    cursor->xcursor_manager = wlr_xcursor_manager_create(get_xcursor_theme_name(), 24);
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

    //let go of container
    if (cursor->grab_container != NULL)
    {
        cursor->grab_container = NULL;
        SIGNAL_DISCONNECT(cursor->grab_container_destroy);
    }
}

// Start grabbing a container under the given mode. (should be RESIZE or MOVE)
static void e_cursor_start_grab_container_mode(struct e_cursor* cursor, struct e_container* container, enum e_cursor_mode mode)
{
    assert(cursor);

    if (container == NULL)
        return;

    e_cursor_set_mode(cursor, mode);

    cursor->grab_container = container;
    cursor->grab_start_cbox = container->area;

    cursor->grab_start_x = cursor->wlr_cursor->x;
    cursor->grab_start_y = cursor->wlr_cursor->y;

    cursor->grab_start_tile_percentage = container->percentage;

    SIGNAL_CONNECT(container->events.destroy, cursor->grab_container_destroy, e_cursor_handle_grab_container_destroy);

    e_log_info("Grabbed container");

    #if E_VERBOSE
    e_log_info("Cursor grab position: XY(%f, %f)", cursor->grab_start_x, cursor->grab_start_y);
    e_log_info("Container area: XY(%i, %i); WH(%i, %i)", container->area.x, container->area.y, container->area.width, container->area.height);
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

// Starts grabbing a container under the resize mode, resizing along specified edges.
// edges is bitmask of enum wlr_edges.
void e_cursor_start_container_resize(struct e_cursor* cursor, struct e_container* container, uint32_t edges)
{
    assert(cursor && container);

    if (container == NULL)
        return;

    if (container->fullscreen)
    {
        e_log_error("e_cursor_start_container_resize: can't resize containers in fullscreen mode!");
        return;
    }

    cursor->grab_edges = edges;

    e_cursor_start_grab_container_mode(cursor, container, E_CURSOR_MODE_RESIZE);

    e_cursor_set_xcursor_resize(cursor, edges);
}

void e_cursor_start_container_move(struct e_cursor* cursor, struct e_container* container)
{
    assert(cursor && container);

    if (container == NULL)
        return;

    if (container->fullscreen)
    {
        e_log_error("e_cursor_start_container_move: can't move containers in fullscreen mode!");
        return;
    }

    e_cursor_start_grab_container_mode(cursor, container, E_CURSOR_MODE_MOVE);
}

// Sets seat focus to whatever surface is under cursor.
// If nothing is under cursor, doesn't change seat focus.
void e_cursor_set_focus_hover(struct e_cursor* cursor)
{
    //only update focus in default mode
    if (cursor->mode != E_CURSOR_MODE_DEFAULT)
        return;

    struct e_server* server = cursor->seat->server;
    struct e_seat* seat = cursor->seat;

    double sx, sy;
    struct wlr_scene_surface* hover_surface = e_desktop_scene_surface_at(&server->scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &sx, &sy);
    struct e_view* view = (hover_surface == NULL) ? NULL : e_view_try_from_node_ancestors(&hover_surface->buffer->node);

    if (hover_surface != NULL)
    {
        wlr_seat_pointer_notify_enter(seat->wlr_seat, hover_surface->surface, sx, sy); //is only sent once

        //sloppy focus
        e_desktop_set_focus_surface(server, hover_surface->surface);
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

    //let go of container
    if (cursor->grab_container != NULL)
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
