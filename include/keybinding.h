#pragma once

// Keybinding functions

#include <xkbcommon/xkbcommon.h>

#include "types/input/keybind_list.h"

int e_keybinding_bind(struct e_keybind_list* list, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods, const char* command);