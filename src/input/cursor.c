#include "input/cursor.h"

#include <stdint.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include "input/input_manager.h"

#include "input/seat.h"
#include "server.h"
#include "windows/toplevel_window.h"

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

    //send to clients
    wlr_seat_pointer_notify_button(cursor->input_manager->seat->wlr_seat, event->time_msec, event->button, event->state);
}

static void e_cursor_handle_move(struct e_cursor* cursor, uint32_t time_msec)
{
    struct e_server* server = cursor->input_manager->server;
    struct e_seat* seat = cursor->input_manager->seat;

    double sx, sy;
    struct wlr_surface* hover_surface;
    struct e_toplevel_window* toplevel_window = e_toplevel_window_at(&server->scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &hover_surface, &sx, &sy);

    if (hover_surface != NULL)
    {
        wlr_seat_pointer_notify_enter(seat->wlr_seat, hover_surface, sx, sy); //is only sent once
        wlr_seat_pointer_notify_motion(seat->wlr_seat, time_msec, sx, sy);

        //sloppy focus
        if (toplevel_window != NULL)
            if (!e_seat_has_focus(seat, toplevel_window->xdg_toplevel->base->surface))
                e_seat_set_focus(seat, toplevel_window->xdg_toplevel->base->surface);
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
    struct e_cursor* cursor = wl_container_of(listener, cursor, motion);
    struct wlr_pointer_axis_event* pointer_axis_event = data;
    
    //send to clients
    wlr_seat_pointer_notify_axis(cursor->input_manager->seat->wlr_seat, pointer_axis_event->time_msec, pointer_axis_event->orientation, pointer_axis_event->delta, pointer_axis_event->delta_discrete, pointer_axis_event->source, pointer_axis_event->relative_direction);
}

struct e_cursor* e_cursor_create(struct e_input_manager* input_manager, struct wlr_output_layout* output_layout)
{
    struct e_cursor* cursor = calloc(1, sizeof(struct e_cursor));
    cursor->input_manager = input_manager;

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