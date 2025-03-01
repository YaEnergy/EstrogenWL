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
#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "desktop/layers/layer_shell.h"

#include "input/seat.h"
#include "server.h"

#include "desktop/scene.h"
#include "desktop/windows/window.h"

#include "util/log.h"

//xcursor names: https://www.freedesktop.org/wiki/Specifications/cursor-spec/

//TODO: these should probably be replaced by a method of founding out the mouse button number
#define E_POINTER_BUTTON_MIDDLE 274
#define E_POINTER_BUTTON_RIGHT 273
#define E_POINTER_BUTTON_LEFT 272

//checks whether this surface should be focussed on by the seat, and sets the seat focus if necessary
static void e_cursor_update_seat_focus(struct e_cursor* cursor, struct wlr_surface* surface)
{
    assert(cursor && surface);

    struct e_server* server = cursor->seat->server;
    struct e_seat* seat = cursor->seat;

    //focus on windows

    struct wlr_surface* root_surface = wlr_surface_get_root_surface(surface);
    struct e_window* window = e_window_from_surface(server, root_surface);

    if (window != NULL && !e_seat_has_focus(seat, window->surface))
    {
        e_seat_set_focus(seat, window->surface, false);
        return;
    }

    //focus on layer surfaces that request on demand interactivity

    struct wlr_layer_surface_v1* hover_layer_surface = wlr_layer_surface_v1_try_from_wlr_surface(surface);

    //is layer surface that requests on demand focus?
    if (hover_layer_surface != NULL && hover_layer_surface->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND)
    {
        if (!e_seat_has_focus(seat, hover_layer_surface->surface))
            e_seat_set_focus(seat, hover_layer_surface->surface, false);    

        return;  
    }
}

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
        //right click is held, start resizing the focussed window (right & bottom edge)
        if (event->button == E_POINTER_BUTTON_RIGHT && event->state == WL_POINTER_BUTTON_STATE_PRESSED)
        {
            struct e_window* focussed_window = e_window_from_surface(cursor->seat->server, cursor->seat->focus_surface);

            e_cursor_start_window_resize(cursor, focussed_window, WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
            handled = true;
        }
        //middle click is held, start moving the focussed window
        else if (event->button == E_POINTER_BUTTON_MIDDLE && event->state == WL_POINTER_BUTTON_STATE_PRESSED)
        {
            struct e_window* focussed_window = e_window_from_surface(cursor->seat->server, cursor->seat->focus_surface);

            e_cursor_start_window_move(cursor, focussed_window);
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
    if (cursor->grab_window == NULL || cursor->grab_window->tree == NULL || cursor->grab_window->container == NULL)
    {
        e_log_error("Cursor move mode: window/window's scene tree grabbed by cursor is NULL");
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
        e_container_set_position(cursor->grab_window->container, cursor->wlr_cursor->x - cursor->grab_wcx, cursor->wlr_cursor->y - cursor->grab_wcy);
    }
}

static void e_cursor_handle_mode_resize(struct e_cursor* cursor)
{   
    if (cursor->grab_window == NULL || cursor->grab_window->container == NULL)
    {
        e_log_error("Cursor resize mode: window/window's container grabbed by cursor is NULL");
        e_cursor_reset_mode(cursor);
        return;
    }

    //TODO: most likely due to some imprecision of some kind (idk), there seems to be movement on the right edge and bottom edge when resizing their opposite edges
    //TODO: wait for window to finish committing before resizing again

    if (cursor->grab_window->tiled)
    {
        //TODO: allow resizing of tiled windows
        wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "not-allowed");
    }
    else //floating
    {
        int left = cursor->grab_start_wcbox.x;
        int right = cursor->grab_start_wcbox.x + cursor->grab_start_wcbox.width;
        int top = cursor->grab_start_wcbox.y;
        int bottom = cursor->grab_start_wcbox.y + cursor->grab_start_wcbox.height;

        //delta x & delta y since start grab (pos x - (grab window container pos x + grabbed window container left border), y and top border for delta_y or grow_y)
        int grow_x = cursor->wlr_cursor->x - cursor->grab_wcx - left;
        int grow_y = cursor->wlr_cursor->y - cursor->grab_wcy - top;

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

        e_container_configure(cursor->grab_window->container, left, top, right - left, bottom - top);
        e_container_arrange(cursor->grab_window->container);
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

    struct e_server* server = cursor->seat->server;
    struct e_seat* seat = cursor->seat;

    double sx, sy;
    struct wlr_scene_node* hover_node;
    struct wlr_surface* hover_surface = e_scene_wlr_surface_at(&server->scene->wlr_scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &hover_node, &sx, &sy);
    struct e_window* window = (hover_node == NULL) ? NULL : e_window_try_from_node_ancestors(hover_node);

    if (hover_surface != NULL)
    {
        wlr_seat_pointer_notify_enter(seat->wlr_seat, hover_surface, sx, sy); //is only sent once
        wlr_seat_pointer_notify_motion(seat->wlr_seat, time_msec, sx, sy);

        //sloppy focus
        e_cursor_update_seat_focus(cursor, hover_surface);
    }
    else 
    {
        wlr_seat_pointer_notify_clear_focus(seat->wlr_seat);
    }

    //display default cursor when not hovering any WINDOWS (not just any surface)
    if (window == NULL)
        wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "default");
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
    wlr_seat_pointer_notify_axis(cursor->seat->wlr_seat, event->time_msec, event->orientation, event->delta, event->delta_discrete, event->source, event->relative_direction);
}

struct e_cursor* e_cursor_create(struct e_seat* seat, struct wlr_output_layout* output_layout)
{
    struct e_cursor* cursor = calloc(1, sizeof(*cursor));
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
    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "default");
}

//start grabbing a window under the given mode, (should be RESIZE or MOVE)
static void e_cursor_start_grab_window_mode(struct e_cursor* cursor, struct e_window* window, enum e_cursor_mode mode)
{
    if (window == NULL || window->container == NULL)
        return;

    e_cursor_set_mode(cursor, mode);

    cursor->grab_window = window;
    cursor->grab_wcx = cursor->wlr_cursor->x - window->container->tree->node.x;
    cursor->grab_wcy = cursor->wlr_cursor->y - window->container->tree->node.y;
}

static void e_cursor_set_xcursor_resize(struct e_cursor* cursor, enum wlr_edges edges)
{
    int direction_i = 0;
    char* direction = calloc(5, sizeof(*direction));

    //alloc fail
    if (direction == NULL)
    {
        e_log_error("e_cursor_set_xcursor_resize: failed to alloc char* direction");
        return;
    }
    
    if (edges & WLR_EDGE_TOP)
    {
        direction[direction_i] = 'n';
        direction_i++;
    }
        
    if (edges & WLR_EDGE_BOTTOM)
    {
        direction[direction_i] = 's';
        direction_i++;
    }

    if (edges & WLR_EDGE_RIGHT)
    {
        direction[direction_i] = 'e';
        direction_i++;
    }
        
    if (edges & WLR_EDGE_LEFT)
    {
        direction[direction_i] = 'w';
        direction_i++;
    }

    direction[direction_i] = '\0'; //null terminator
    direction_i++;
    
    //no edges can be resized or more than 2 directions
    if (direction_i <= 1 || direction_i >= 4)
    {
        free(direction);
        wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, "not-allowed");
        return;
    }

    const int MAX_NAME_LENGTH = 11; // 2 direction chars + "-resize" + '\0'

    char* resize_cursor_name = calloc(MAX_NAME_LENGTH, sizeof(*resize_cursor_name));

    //alloc fail
    if (resize_cursor_name == NULL)
    {
        free(direction);
        e_log_error("e_cursor_set_xcursor_resize: failed to alloc char* resize_cursor_name");
        return;
    }

    int length = snprintf(resize_cursor_name, MAX_NAME_LENGTH, "%s-resize", direction);
    
    free(direction);

    if (length > MAX_NAME_LENGTH)
    {
        free(resize_cursor_name);
        return;
    }

    wlr_cursor_set_xcursor(cursor->wlr_cursor, cursor->xcursor_manager, resize_cursor_name);

    free(resize_cursor_name);
}

void e_cursor_start_window_resize(struct e_cursor* cursor, struct e_window* window, enum wlr_edges edges)
{
    if (window == NULL || window->container == NULL)
        return;

    cursor->grab_start_wcbox = window->container->area;
    cursor->grab_edges = edges;

    e_cursor_start_grab_window_mode(cursor, window, E_CURSOR_MODE_RESIZE);

    e_cursor_set_xcursor_resize(cursor, edges);
}

void e_cursor_start_window_move(struct e_cursor* cursor, struct e_window* window)
{
    if (window == NULL || window->tree == NULL)
        return;

    e_cursor_start_grab_window_mode(cursor, window, E_CURSOR_MODE_MOVE);
}

void e_cursor_update_focus(struct e_cursor *cursor)
{
    //only update focus in default mode
    if (cursor->mode != E_CURSOR_MODE_DEFAULT)
        return;

    struct e_server* server = cursor->seat->server;
    struct e_seat* seat = cursor->seat;

    double sx, sy;
    struct wlr_scene_node* hover_node;
    struct wlr_surface* hover_surface = e_scene_wlr_surface_at(&server->scene->wlr_scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y, &hover_node, &sx, &sy);
    struct e_window* window = (hover_node == NULL) ? NULL : e_window_try_from_node_ancestors(hover_node);

    if (hover_surface != NULL)
    {
        wlr_seat_pointer_notify_enter(seat->wlr_seat, hover_surface, sx, sy); //is only sent once

        //sloppy focus
        e_cursor_update_seat_focus(cursor, hover_surface);
    }
    else 
    {
        wlr_seat_pointer_notify_clear_focus(seat->wlr_seat);
    }

    //display default cursor when not hovering any WINDOWS (not just any surface)
    if (window == NULL)
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