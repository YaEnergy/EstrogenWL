#include "types/input/seat.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>

#include "types/input/keyboard.h"
#include "types/server.h"
#include "log.h"

//2024-12-18 22:29:22 | starting to be able to do this more on my own now, I feel like I'm learning a lot :3

//new input found on the backend
static void e_seat_new_input(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, new_input);
    struct wlr_input_device* input = data;

    switch(input->type)
    {
        case WLR_INPUT_DEVICE_KEYBOARD:
            e_log_info("new keyboard input device");

            //create e_keyboard and add it to list of keyboards

            struct wlr_keyboard* wlr_keyboard = wlr_keyboard_from_input_device(input);
            struct e_keyboard* keyboard = e_keyboard_create(wlr_keyboard);
            
            wl_list_insert(&seat->keyboards, &keyboard->link);

            break;
        case WLR_INPUT_DEVICE_POINTER:
            e_log_info("new pointer input device");

            //TODO: implement pointer device stuff

            break;
        default:
            e_log_info("new unsupported input device, ignoring...");
            break;
    }
}

static void e_seat_destroy(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, destroy);

    wl_list_remove(&seat->new_input.link);
    wl_list_remove(&seat->destroy.link);

    free(seat);
}

struct e_seat* e_seat_create(struct wl_display* display, struct wlr_backend* backend, const char* name)
{
    struct e_seat* seat = calloc(1, sizeof(struct e_seat));

    struct wlr_seat* wlr_seat = wlr_seat_create(display, name);

    seat->wlr_seat = wlr_seat;

    wl_list_init(&seat->keyboards);

    //listen for new input devices on backend
    seat->new_input.notify = e_seat_new_input;
    wl_signal_add(&backend->events.new_input, &seat->new_input);

    //events
    seat->destroy.notify = e_seat_destroy;
    wl_signal_add(&wlr_seat->events.destroy, &seat->destroy);

    return seat;
}

