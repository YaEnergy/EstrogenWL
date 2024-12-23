#include "input/seat.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>

#include "server.h"
#include "input/input_manager.h"
#include "input/keyboard.h"

#include "util/log.h"

//2024-12-18 22:29:22 | starting to be able to do this more on my own now, I feel like I'm learning a lot :3

static void e_seat_destroy(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, destroy);

    wl_list_remove(&seat->keyboards);

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
    seat->destroy.notify = e_seat_destroy;
    wl_signal_add(&wlr_seat->events.destroy, &seat->destroy);

    return seat;
}

// focus

void e_seat_set_focus(struct e_seat* seat, struct wlr_surface* surface)
{
    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    //set active keyboard focus to surface
    //only if there is an active keyboard and this keyboard doesn't already have focus on the surface
    if (wlr_keyboard != NULL && seat->wlr_seat->keyboard_state.focused_surface != surface)
        wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface, wlr_keyboard->keycodes, wlr_keyboard->num_keycodes, &wlr_keyboard->modifiers);
    
    seat->focus_surface = surface;

    e_log_info("seat focus");
}

//returns true if seat has full focus on this surface
//(all active devices have focus on this surface)
bool e_seat_has_focus(struct e_seat* seat, struct wlr_surface* surface)
{
    if (seat->focus_surface != surface)
        return false;

    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    if (wlr_keyboard != NULL && seat->wlr_seat->keyboard_state.focused_surface != surface)
        return false;

    return true;
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

    e_seat_update_capabilities(seat);
}