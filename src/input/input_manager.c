#include "input/input_manager.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include "server.h"

#include "input/seat.h"
#include "input/keybind_list.h"
#include "input/cursor.h"

#include "util/log.h"

static void e_input_manager_new_input(struct wl_listener* listener, void* data)
{
    struct e_input_manager* input_manager = wl_container_of(listener, input_manager, new_input);

    struct wlr_input_device* input = data;

    switch(input->type)
    {
        case WLR_INPUT_DEVICE_KEYBOARD:
            e_log_info("new keyboard input device");

            e_seat_add_keyboard(input_manager->seat, input);
            break;
        case WLR_INPUT_DEVICE_POINTER:
            e_log_info("new pointer input device");

            //attach pointer to cursor
            wlr_cursor_attach_input_device(input_manager->cursor->wlr_cursor, input);
            
            break;
        default:
            e_log_info("new unsupported input device, ignoring...");
            break;
    }
}

struct e_input_manager* e_input_manager_create(struct e_server* server)
{
    struct e_input_manager* input_manager = calloc(1, sizeof(struct e_input_manager));

    input_manager->server = server;
    
    input_manager->keybind_list = e_keybind_list_create();

    input_manager->cursor = e_cursor_create(server->output_layout);

    //listen for new input devices on backend
    input_manager->new_input.notify = e_input_manager_new_input;
    wl_signal_add(&server->backend->events.new_input, &input_manager->new_input);

    input_manager->seat = e_seat_create(input_manager, "seat0");

    return input_manager;
}

void e_input_manager_destroy(struct e_input_manager* input_manager)
{
    e_cursor_destroy(input_manager->cursor);
    
    e_keybind_list_destroy(&input_manager->keybind_list);

    wl_list_remove(&input_manager->new_input.link);

    free(input_manager);
}