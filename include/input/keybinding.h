#pragma once

// Keybinding functions

#include <xkbcommon/xkbcommon.h>

#include "server.h"
#include "input/keybind_list.h"

int e_keybinding_bind(struct e_keybind_list* list, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods, const char* command);

bool e_keybinding_handle(struct e_server* server, struct e_keybind_list* keybinds, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods);