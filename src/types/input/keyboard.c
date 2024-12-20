#include "types/input/keyboard.h"
#include "log.h"
#include "types/input/seat.h"

#include <stdbool.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <wlr/types/wlr_keyboard.h>

#include "types/input/keybind_list.h"
#include "types/input/keybind.h"

#include "commands.h"
#include "log.h"

//key pressed or released, emitted before keyboard xkb state is updated (including modifiers)
static void e_keyboard_key(struct wl_listener* listener, void* data)
{
    struct e_keyboard* keyboard = wl_container_of(listener, keyboard, key);
    struct wlr_keyboard_key_event* event = data;
    
    //TODO: for testing rn always passes alt modifier

    //TODO: handle keybinds, if none activated, send inputs to focussed client
    //translate libinput keycode to xkbcommon keycode
    uint32_t xkb_keycode = event->keycode + 8;

    bool handled = false;

    //handle keybinds if any
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED)
    {
        //int keybindIndex = e_keybind_list_should_activate_index_of(keyboard->seat->keybind_list, xkb_keycode, wlr_keyboard_get_modifiers(keyboard->wlr_keyboard));
        int keybindIndex = e_keybind_list_should_activate_index_of(keyboard->seat->input_manager->keybind_list, xkb_keycode, WLR_MODIFIER_ALT);

        if (keybindIndex != -1)
        {
            handled = true;
            e_commands_parse(keyboard->seat->input_manager->server, e_keybind_list_at(keyboard->seat->input_manager->keybind_list, keybindIndex).command);
            e_log_info("activated keybind index %i", keybindIndex);
        }
    }
    
    //send to clients if not handled
    if (!handled)
    {
        wlr_seat_set_keyboard(keyboard->seat->wlr_seat, keyboard->wlr_keyboard);
        wlr_seat_keyboard_notify_key(keyboard->seat->wlr_seat, event->time_msec, event->keycode, event->state);
    }

    e_log_info("Keycode: %lu; State: %lu; Handled: %i, modifiers: %lu", event->keycode, event->state, handled, wlr_keyboard_get_modifiers(keyboard->wlr_keyboard));
}

static void e_keyboard_modifiers(struct wl_listener* listener, void* data)
{
    struct e_keyboard* keyboard = wl_container_of(listener, keyboard, modifiers);

    wlr_seat_set_keyboard(keyboard->seat->wlr_seat, keyboard->wlr_keyboard);
    wlr_seat_keyboard_notify_modifiers(keyboard->seat->wlr_seat, &keyboard->wlr_keyboard->modifiers);
}

static void e_keyboard_destroy(struct wl_listener* listener, void* data)
{
    struct e_keyboard* keyboard = wl_container_of(listener, keyboard, destroy);

    wl_list_remove(&keyboard->link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->destroy.link);

    free(keyboard);
}

struct e_keyboard* e_keyboard_create(struct wlr_input_device* input, struct e_seat* seat)
{
    struct wlr_keyboard* wlr_keyboard = wlr_keyboard_from_input_device(input);
    
    struct e_keyboard* keyboard = calloc(1, sizeof(struct e_keyboard));
    keyboard->seat = seat;
    keyboard->wlr_keyboard = wlr_keyboard;

    //TODO: keymap?

    wl_list_init(&keyboard->link);
    
    //keyboard events
    keyboard->key.notify = e_keyboard_key;
    wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);

    keyboard->modifiers.notify = e_keyboard_modifiers;
    wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);

    //input device events
    keyboard->destroy.notify = e_keyboard_destroy;
    wl_signal_add(&wlr_keyboard->base.events.destroy, &keyboard->destroy);

    return keyboard;
}