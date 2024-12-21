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

    wl_list_init(&seat->keyboards);

    //events
    seat->destroy.notify = e_seat_destroy;
    wl_signal_add(&wlr_seat->events.destroy, &seat->destroy);

    return seat;
}


void e_seat_add_keyboard(struct e_seat* seat, struct wlr_input_device* input)
{
    e_log_info("new keyboard input device");

    struct e_keyboard* keyboard = e_keyboard_create(input, seat);

    //add to seat
    wl_list_insert(&seat->keyboards, &keyboard->link);
    wlr_seat_set_keyboard(seat->wlr_seat, keyboard->wlr_keyboard);

    //send to clients what the capabilities of this seat are

    //seat capabilities mask
    uint32_t capabilities = 0;

    //keyboard available?
    if (!wl_list_empty(&seat->keyboards))
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;

    wlr_seat_set_capabilities(seat->wlr_seat, capabilities);
}