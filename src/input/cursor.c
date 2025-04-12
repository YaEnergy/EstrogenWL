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
#include <wlr/types/wlr_layer_shell_v1.h>

#include <wlr/util/box.h>
#include <wlr/util/edges.h>

#include "desktop/tree/container.h"
#include "desktop/views/window.h"

#include "input/seat.h"

#include "desktop/desktop.h"
#include "desktop/views/view.h"

#include "util/log.h"

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
        //right click is held, start resizing the focussed view (right & bottom edge)
        if (event->button == E_POINTER_BUTTON_RIGHT && event->state == WL_POINTER_BUTTON_STATE_PRESSED)
        {
            struct e_view* focused_view = e_view_from_surface(cursor->seat->desktop, cursor->seat->focus_surface);

            e_cursor_start_window_resize(cursor, focused_view->container, WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
            handled = true;
        }
        //middle click is held, start moving the focussed view
        else if (event->button == E_POINTER_BUTTON_MIDDLE && event->state == WL_POINTER_BUTTON_STATE_PRESSED)
        {
            struct e_view* focused_view = e_view_from_surface(cursor->seat->desktop, cursor->seat->focus_surface);

            e_cursor_start_window_move(cursor, focused_view->container);
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

static void e_cursor_handle_mode_move(struct e_cursor* cursor)
{
    if (cursor->grab_window == NULL)
    {
        e_log_error("Cursor move mode: window grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    if (cursor->grab_window->tiled)
    {
        //TODO: allow positioning of tiled windows
        wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "not-allowed");
    }
    else //floating
    {
        wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "all-scroll");
        e_window_set_position(cursor->grab_window, cursor->wlr_cursor->x - cursor->grab_wx, cursor->wlr_cursor->y - cursor->grab_wy);
    }
}

static void e_cursor_handle_mode_resize(struct e_cursor* cursor)
{   
    if (cursor->grab_window == NULL)
    {
        e_log_error("Cursor resize mode: window grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    //TODO: most likely due to some imprecision of some kind (idk), there seems to be movement on the right edge and bottom edge when resizing their opposite edges
    //TODO: wait for view to finish committing before resizing again

    if (cursor->grab_window->tiled)
    {
        //TODO: allow resizing of tiled views
        wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "not-allowed");
    }
    else //floating
    {
        int left = cursor->grab_start_wbox.x;
        int right = cursor->grab_start_wbox.x + cursor->grab_start_wbox.width;
        int top = cursor->grab_start_wbox.y;
        int bottom = cursor->grab_start_wbox.y + cursor->grab_start_wbox.height;

        //delta x & delta y since start grab (pos x - (grab window pos x + grabbed window left border), y and top border for delta_y or grow_y)
        int grow_x = cursor->wlr_cursor->x - cursor->grab_wx - left;
        int grow_y = cursor->wlr_cursor->y - cursor->grab_wy - top;

        //expand grabbed edges, while not letting them overlap the opposite edge, min 1 pixel

        if (cursor->grab_edges & WLR_EDGE_LEFT)
        {
            left += grow_x;

            if (left > right - 1)
                left = right - 1;
        }
        else if (cursor->grab_edges & WLR_EDGE_RIGHT)
        {
            right += grow_x;

            if (right < left + 1)
                right = left + 1;
        }

        if (cursor->grab_edges & WLR_EDGE_TOP)
        {
            top += grow_y;

            if (top > bottom - 1)
                top = bottom - 1;
        }
        else if (cursor->grab_edges & WLR_EDGE_BOTTOM)
        {
            bottom += grow_y;

            if (bottom < top + 1)
                bottom = top + 1;
        }

        e_window_configure(cursor->grab_window, left, top, right - left, bottom - top);
    }
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
    struct wlr_scene_node* hover_node;
    struct wlr_surface* hover_surface = e_desktop_wlr_surface_at(&desktop->scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &hover_node, &sx, &sy);

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

// Grabbed window was destroyed, let go.
static void e_cursor_grab_window_destroy(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, grab_window_destroy);

    e_log_info("grabbed window was destroyed");

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

    //boundaries and movement semantics of cursor
    wlr_cursor_attach_output_layout(cursor->wlr_cursor, output_layout);

    //load xcursor theme
    cursor->xcursor_manager = wlr_xcursor_manager_create(NULL, 24);
    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "default");

    // events

    cursor->frame.notify = e_cursor_frame;
    wl_signal_add(&cursor->wlr_cursor->events.frame, &cursor->frame);

    cursor->button.notify = e_cursor_button;
    wl_signal_add(&cursor->wlr_cursor->events.button, &cursor->button);

    cursor->motion.notify = e_cursor_motion;
    wl_signal_add(&cursor->wlr_cursor->events.motion, &cursor->motion);

    cursor->motion_absolute.notify = e_cursor_motion_absolute;
    wl_signal_add(&cursor->wlr_cursor->events.motion_absolute, &cursor->motion_absolute);

    cursor->axis.notify = e_cursor_axis;
    wl_signal_add(&cursor->wlr_cursor->events.axis, &cursor->axis);

    cursor->grab_window_destroy.notify = e_cursor_grab_window_destroy;

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

    //let go of window
    if (cursor->grab_window != NULL)
    {
        cursor->grab_window = NULL;
        wl_list_remove(&cursor->grab_window_destroy.link);
    }
}

// Start grabbing a window under the given mode. (should be RESIZE or MOVE)
static void e_cursor_start_grab_window_mode(struct e_cursor* cursor, struct e_window* window, enum e_cursor_mode mode)
{
    assert(cursor);

    if (window == NULL)
        return;

    e_cursor_set_mode(cursor, mode);

    cursor->grab_window = window;
    cursor->grab_wx = cursor->wlr_cursor->x - window->base.tree->node.x;
    cursor->grab_wy = cursor->wlr_cursor->y - window->base.tree->node.y;
    cursor->grab_start_wbox = window->base.area;

    wl_signal_add(&window->events.destroy, &cursor->grab_window_destroy);

    e_log_info("Grabbed window");

    #if E_VERBOSE
    e_log_info("Window grab position: XY(%f, %f)", cursor->grab_wx, cursor->grab_wy);
    e_log_info("Container rect: XY(%i, %i); WH(%i, %i)", window->base.area.x, window->base.area.y, window->base.area.width, window->base.area.height);
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

void e_cursor_start_window_resize(struct e_cursor* cursor, struct e_window* window, enum wlr_edges edges)
{
    if (window == NULL)
        return;

    cursor->grab_edges = edges;

    e_cursor_start_grab_window_mode(cursor, window, E_CURSOR_MODE_RESIZE);

    e_cursor_set_xcursor_resize(cursor, edges);
}

void e_cursor_start_window_move(struct e_cursor* cursor, struct e_window* window)
{
    assert(cursor);

    if (window == NULL)
        return;

    e_cursor_start_grab_window_mode(cursor, window, E_CURSOR_MODE_MOVE);
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
    struct wlr_scene_node* hover_node;
    struct wlr_surface* hover_surface = e_desktop_wlr_surface_at(&desktop->scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &hover_node, &sx, &sy);
    struct e_view* view = (hover_node == NULL) ? NULL : e_view_try_from_node_ancestors(hover_node);

    if (hover_surface != NULL)
    {
        wlr_seat_pointer_notify_enter(seat->wlr_seat, hover_surface, sx, sy); //is only sent once

        //sloppy focus
        e_seat_set_focus_surface_type(seat, hover_surface);
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
    wl_list_remove(&cursor->frame.link);
    wl_list_remove(&cursor->button.link);
    wl_list_remove(&cursor->motion.link);
    wl_list_remove(&cursor->motion_absolute.link);
    wl_list_remove(&cursor->axis.link);

    wlr_xcursor_manager_destroy(cursor->xcursor_manager);
    wlr_cursor_destroy(cursor->wlr_cursor);

    free(cursor);
}