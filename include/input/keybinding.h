#pragma once

// Keybinding functions

#include <xkbcommon/xkbcommon.h>

#include "server.h"

#include "util/list.h"

// Returns true on success, false on fail.
bool e_keybinding_bind(struct e_list* list, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods, const char* command);

// Returns true if a keybind was activated, false if not.
bool e_keybinding_handle(struct e_server* server, struct e_list* keybinds, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods);