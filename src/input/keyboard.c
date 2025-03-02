#include "input/keyboard.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <wlr/types/wlr_keyboard.h>

#include <xkbcommon/xkbcommon.h>

#include "input/seat.h"
#include "input/keybind.h"

#include "util/list.h"

#include "commands.h"

bool e_server_handle_keybind(struct e_server* server, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods)
{
    struct e_list* keybinds = server->seat->keybinds;

    for (int i = 0; i < keybinds->count; i++)
    {
        struct e_keybind* keybind = e_list_at(keybinds, i);

        if (e_keybind_should_activate(keybind, keysym, mods))
        {
            e_commands_parse(server, keybind->command);
            return true;
        }
    }

    return false;
}

//key pressed or released, emitted before keyboard xkb state is updated (including modifiers)
static void e_keyboard_key(struct wl_listener* listener, void* data)
{
    struct e_keyboard* keyboard = wl_container_of(listener, keyboard, key);
    struct wlr_keyboard_key_event* event = data;

    bool handled = false;

    //handle keybinds if any
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED)
    {
        //translate libinput keycode to xkbcommon keycode
        uint32_t xkb_keycode = event->keycode + 8;

        uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);

        //list of syms based on this keymap for this keyboard
        const xkb_keysym_t* syms;
        int num_syms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, xkb_keycode, &syms);

        for (int i = 0; i < num_syms; i++)
        {
            if (e_server_handle_keybind(keyboard->seat->server, syms[i], modifiers))
                handled = true;
        }
    }
    
    //send to clients if not handled
    if (!handled)
    {
        wlr_seat_set_keyboard(keyboard->seat->wlr_seat, keyboard->wlr_keyboard);
        wlr_seat_keyboard_notify_key(keyboard->seat->wlr_seat, event->time_msec, event->keycode, event->state);
    }
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
    
    struct e_keyboard* keyboard = calloc(1, sizeof(*keyboard));
    keyboard->seat = seat;
    keyboard->wlr_keyboard = wlr_keyboard;

    //TODO: allow configuring of keyboard xkb keymaps
    //set up keymap (DEFAULT: US)
    struct xkb_context* xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap* keymap = xkb_keymap_new_from_names(xkb_context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(wlr_keyboard, keymap);

    xkb_keymap_unref(keymap);
    xkb_context_unref(xkb_context);

    //TODO: allow configuring of keyboard repeat info
    wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

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