#include "types/input/keyboard.h"
#include "log.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_keyboard.h>

//key pressed or released, emitted before keyboard xkb state is updated (including modifiers)
static void e_keyboard_key(struct wl_listener* listener, void* data)
{
    struct e_keyboard* keyboard = wl_container_of(listener, keyboard, key);
    struct wlr_keyboard_key_event* event = data;

    //TODO: handle keybinds, if none activated, send inputs to focussed client
    
    e_log_info("Keycode: %lu; State: %lu", event->keycode, event->state);
}

static void e_keyboard_destroy(struct wl_listener* listener, void* data)
{
    struct e_keyboard* keyboard = wl_container_of(listener, keyboard, destroy);

    wl_list_remove(&keyboard->link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->destroy.link);

    free(keyboard);
}

struct e_keyboard* e_keyboard_create(struct wlr_keyboard* wlr_keyboard)
{
    struct e_keyboard* keyboard = calloc(1, sizeof(struct e_keyboard));
    keyboard->wlr_keyboard = wlr_keyboard;

    //TODO: keymap?

    wl_list_init(&keyboard->link);
    
    //keyboard events
    keyboard->key.notify = e_keyboard_key;
    wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);

    //input device events
    keyboard->destroy.notify = e_keyboard_destroy;
    wl_signal_add(&wlr_keyboard->base.events.destroy, &keyboard->destroy);

    return keyboard;
}