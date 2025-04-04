#pragma once

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_keyboard.h>

#include "input/seat.h"

struct e_keyboard
{
    struct e_seat* seat;
    struct wlr_keyboard* wlr_keyboard;

    //keyboard events

    //key pressed or released, emitted before keyboard xkb state is updated (including modifiers)
    struct wl_listener key;
    //modifier state changed, handle updated state by sending it to clients
    struct wl_listener modifiers;

    //base input device events

    //destroy keyboard
    struct wl_listener destroy;

    struct wl_list link;
};

// Returns NULL on fail.
struct e_keyboard* e_keyboard_create(struct wlr_input_device* input, struct e_seat* seat);