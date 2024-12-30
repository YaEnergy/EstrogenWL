#include "input/cursor.h"

#include <stdint.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include <wlr/util/box.h>
#include <wlr/util/edges.h>

#include "desktop/xdg_shell.h"
#include "input/input_manager.h"

#include "input/seat.h"
#include "server.h"

#include "desktop/scene.h"
#include "desktop/windows/window.h"

#include "util/log.h"

//TODO: these should probably be replaced by a method of founding out the mouse button number
#define E_POINTER_BUTTON_MIDDLE 274
#define E_POINTER_BUTTON_RIGHT 273
#define E_POINTER_BUTTON_LEFT 272

static void e_cursor_frame(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, frame);
    
    wlr_seat_pointer_notify_frame(cursor->input_manager->seat->wlr_seat);
}

//mouse button presses
static void e_cursor_button(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, button);
    struct wlr_pointer_button_event* event = data;

    if (event->button == E_POINTER_BUTTON_MIDDLE && event->state == WL_POINTER_BUTTON_STATE_PRESSED)
    {
        struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(cursor->input_manager->seat->wlr_seat);

        //if ALT modifier is pressed on keyboard & right click is held, start resizing the focussed window (right & bottom edge)
        if (keyboard != NULL && (wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_ALT))
        {
            struct e_window* focussed_window = e_window_from_surface(cursor->input_manager->server, cursor->input_manager->seat->focus_surface);

            e_cursor_start_window_resize(cursor, focussed_window, WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
        }
    }

    if (event->state == WL_POINTER_BUTTON_STATE_RELEASED)
        e_cursor_reset_mode(cursor);

    //send to clients
    wlr_seat_pointer_notify_button(cursor->input_manager->seat->wlr_seat, event->time_msec, event->button, event->state);
}

static void e_cursor_handle_mode_move(struct e_cursor* cursor)
{
    if (cursor->grab_window == NULL || cursor->grab_window->scene_tree == NULL)
    {
        e_log_error("Cursor move mode: window/window's scene tree grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    //TODO: allow positioning of tiled windows

    if (!cursor->grab_window->tiled)
        e_window_set_position(cursor->grab_window, cursor->wlr_cursor->x - cursor->grab_wx, cursor->wlr_cursor->y - cursor->grab_wy);
}

static void e_cursor_handle_mode_resize(struct e_cursor* cursor)
{   
    if (cursor->grab_window == NULL || cursor->grab_window->scene_tree == NULL)
    {
        e_log_error("Cursor resize mode: window/window's scene tree grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    //TODO: allow resizing of tiled windows
    //TODO: most likely due to some imprecision of some kind (idk), there seems to be movement on the right edge and bottom edge when resizing their opposite edges
    //TODO: wait for window to finish committing before resizing again

    if (!cursor->grab_window->tiled)
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
            left = left + grow_x;

            if (left > right - 1)
                left = right - 1;
        }
        else if (cursor->grab_edges & WLR_EDGE_RIGHT)
        {
            right = right + grow_x;

            if (right < left + 1)
                right = left + 1;
        }

        if (cursor->grab_edges & WLR_EDGE_TOP)
        {
            top = top + grow_y < bottom;

            if (top > bottom - 1)
                top = bottom - 1;
        }
        else if (cursor->grab_edges & WLR_EDGE_BOTTOM)
        {
            bottom = bottom + grow_y;

            if (bottom < top + 1)
                bottom = top + 1;
        }
        
        e_window_set_position(cursor->grab_window, left, top);
        e_window_set_size(cursor->grab_window, right - left, bottom - top);
    }
}

static void e_cursor_handle_move(struct e_cursor* cursor, uint32_t time_msec)
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

    struct e_server* server = cursor->input_manager->server;
    struct e_seat* seat = cursor->input_manager->seat;

    double sx, sy;
    struct wlr_surface* hover_surface;
    struct e_window* window = e_window_at(&server->scene->wlr_scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &hover_surface, &sx, &sy);

    if (window == NULL)
        wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "default");

    if (hover_surface != NULL)
    {
        wlr_seat_pointer_notify_enter(seat->wlr_seat, hover_surface, sx, sy); //is only sent once
        wlr_seat_pointer_notify_motion(seat->wlr_seat, time_msec, sx, sy);

        //sloppy focus on windows
        if (window != NULL)
        {
            struct wlr_surface* window_surface = e_window_get_surface(window);

            if (!e_seat_has_focus(seat, window_surface))
                e_seat_set_focus(seat, window_surface);
        }
    }
    else 
    {
        wlr_seat_pointer_clear_focus(seat->wlr_seat);
    }
}

static void e_cursor_motion(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, motion);
    struct wlr_pointer_motion_event* event = data;

    //move & send to clients
    wlr_cursor_move(cursor->wlr_cursor, &event->pointer->base, event->delta_x, event->delta_y);

    e_cursor_handle_move(cursor, event->time_msec);
}

static void e_cursor_motion_absolute(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, motion_absolute);
    struct wlr_pointer_motion_absolute_event* event = data;

    //move & send to clients
    wlr_cursor_warp_absolute(cursor->wlr_cursor, &event->pointer->base, event->x, event->y);
    
    e_cursor_handle_move(cursor, event->time_msec);
}

//scroll event
static void e_cursor_axis(struct wl_listener* listener, void* data)
{
    struct e_cursor* cursor = wl_container_of(listener, cursor, axis);
    struct wlr_pointer_axis_event* event = data;
    
    //send to clients
    wlr_seat_pointer_notify_axis(cursor->input_manager->seat->wlr_seat, event->time_msec, event->orientation, event->delta, event->delta_discrete, event->source, event->relative_direction);
}

struct e_cursor* e_cursor_create(struct e_input_manager* input_manager, struct wlr_output_layout* output_layout)
{
    struct e_cursor* cursor = calloc(1, sizeof(struct e_cursor));
    cursor->input_manager = input_manager;
    cursor->mode = E_CURSOR_MODE_DEFAULT;

    cursor->wlr_cursor = wlr_cursor_create();

    //boundaries and movement semantics of cursor
    wlr_cursor_attach_output_layout(cursor->wlr_cursor, output_layout);

    //load "default" xcursor theme
    cursor->xcursor_manager = wlr_xcursor_manager_create("default", 24);
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
    cursor->grab_window = NULL;
}

//start grabbing a window under the given mode, (should be RESIZE or MOVE)
static void e_cursor_start_grab_window_mode(struct e_cursor* cursor, struct e_window* window, enum e_cursor_mode mode)
{
    if (window == NULL || window->scene_tree == NULL)
        return;

    e_cursor_set_mode(cursor, mode);

    cursor->grab_window = window;
    cursor->grab_wx = cursor->wlr_cursor->x - window->scene_tree->node.x;
    cursor->grab_wy = cursor->wlr_cursor->y - window->scene_tree->node.y;
}

void e_cursor_start_window_resize(struct e_cursor* cursor, struct e_window* window, enum wlr_edges edges)
{
    if (window == NULL || window->scene_tree == NULL)
        return;

    cursor->grab_start_wbox = (struct wlr_box){window->scene_tree->node.x, window->scene_tree->node.y, 0, 0};
    e_window_get_size(window, &cursor->grab_start_wbox.width, &cursor->grab_start_wbox.height);

    cursor->grab_edges = edges;

    e_cursor_start_grab_window_mode(cursor, window, E_CURSOR_MODE_RESIZE);
}

void e_cursor_start_window_move(struct e_cursor* cursor, struct e_window* window)
{
    if (window == NULL || window->scene_tree == NULL)
        return;

    e_cursor_start_grab_window_mode(cursor, window, E_CURSOR_MODE_MOVE);
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
}