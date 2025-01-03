#include "input/seat.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "desktop/layer_shell.h"
#include "server.h"
#include "input/input_manager.h"
#include "input/keyboard.h"
#include "input/cursor.h"
#include "desktop/windows/window.h"

#include "util/log.h"

//2024-12-18 22:29:22 | starting to be able to do this more on my own now, I feel like I'm learning a lot :3

static void e_seat_request_set_cursor(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, request_set_cursor);
    struct wlr_seat_pointer_request_set_cursor_event* event = data;

    //any client can request, only allow focused client to actually set the surface of the cursor
    if (event->seat_client == seat->wlr_seat->pointer_state.focused_client)
        wlr_cursor_set_surface(seat->input_manager->cursor->wlr_cursor, event->surface, event->hotspot_x, event->hotspot_y);
}

static void e_seat_request_set_selection(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, request_set_selection);
    struct wlr_seat_request_set_selection_event* event = data;
    
    wlr_seat_set_selection(seat->wlr_seat, event->source, event->serial);
}

static void e_seat_destroy(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, destroy);

    wl_list_remove(&seat->keyboards);

    wl_list_remove(&seat->request_set_cursor.link);
    wl_list_remove(&seat->request_set_selection.link);
    wl_list_remove(&seat->destroy.link);

    free(seat);
}

struct e_seat* e_seat_create(struct e_input_manager* input_manager, const char* name)
{
    struct e_seat* seat = calloc(1, sizeof(struct e_seat));

    struct wlr_seat* wlr_seat = wlr_seat_create(input_manager->server->display, name);

    seat->wlr_seat = wlr_seat;
    seat->input_manager = input_manager;
    seat->focus_surface = NULL;

    wl_list_init(&seat->keyboards);

    //events
    seat->request_set_cursor.notify = e_seat_request_set_cursor;
    wl_signal_add(&wlr_seat->events.request_set_cursor, &seat->request_set_cursor);

    seat->request_set_selection.notify = e_seat_request_set_selection;
    wl_signal_add(&wlr_seat->events.request_set_selection, &seat->request_set_selection);

    seat->destroy.notify = e_seat_destroy;
    wl_signal_add(&wlr_seat->events.destroy, &seat->destroy);

    return seat;
}

// focus

static bool e_seat_has_exclusive_layer_focus(struct e_seat* seat)
{
    if (seat->focus_surface == NULL)
        return false;

    struct wlr_layer_surface_v1* layer_surface_v1 = wlr_layer_surface_v1_try_from_wlr_surface(seat->focus_surface);

    return (layer_surface_v1 != NULL && layer_surface_v1->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
}

void e_seat_set_focus(struct e_seat* seat, struct wlr_surface* surface)
{
    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    //Don't override focus of layer surfaces that request exclusive focus
    if (e_seat_has_exclusive_layer_focus(seat))
        return;

    //set active keyboard focus to surface
    //only if there is an active keyboard and this keyboard doesn't already have focus on the surface
    if (wlr_keyboard != NULL && seat->wlr_seat->keyboard_state.focused_surface != surface)
    {
        //leave currently focused surface
        if (seat->wlr_seat->keyboard_state.focused_surface != NULL)
            wlr_seat_keyboard_notify_clear_focus(seat->wlr_seat);

        wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface, wlr_keyboard->keycodes, wlr_keyboard->num_keycodes, &wlr_keyboard->modifiers);
    }
    
    seat->focus_surface = surface;

    struct e_window* window = e_window_from_surface(seat->input_manager->server, surface);
    
    if (window != NULL && window->scene_tree != NULL)
        wlr_scene_node_raise_to_top(&window->scene_tree->node);

    e_log_info("seat focus");
}

//returns true if seat has focus on this surface
bool e_seat_has_focus(struct e_seat* seat, struct wlr_surface* surface)
{
    if (seat->focus_surface != surface)
        return false;

    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    return (wlr_keyboard != NULL && seat->wlr_seat->keyboard_state.focused_surface == surface);
}

void e_seat_clear_focus(struct e_seat *seat)
{
    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    //if there is an active keyboard, clear its focus
    if (wlr_keyboard != NULL)
        wlr_seat_keyboard_notify_clear_focus(seat->wlr_seat);

    seat->focus_surface = NULL;
}

// input devices

static void e_seat_update_capabilities(struct e_seat* seat)
{
    //send to clients what the capabilities of this seat are

    //seat capabilities mask, always has capability for pointers
    uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER;

    //keyboard available?
    if (!wl_list_empty(&seat->keyboards))
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;

    wlr_seat_set_capabilities(seat->wlr_seat, capabilities);
}

void e_seat_add_keyboard(struct e_seat* seat, struct wlr_input_device* input)
{
    e_log_info("adding keyboard input device");

    struct e_keyboard* keyboard = e_keyboard_create(input, seat);

    //add to seat
    wl_list_insert(&seat->keyboards, &keyboard->link);
    wlr_seat_set_keyboard(seat->wlr_seat, keyboard->wlr_keyboard);
    
    if (seat->focus_surface != NULL)
        e_seat_set_focus(seat, seat->focus_surface);

    e_seat_update_capabilities(seat);
}