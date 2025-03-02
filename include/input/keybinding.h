#pragma once

// Keybinding functions

#include <xkbcommon/xkbcommon.h>

#include <wlr/types/wlr_keyboard.h>

#include "util/list.h"

// Returns true on success, false on fail.
bool e_keybinding_bind(struct e_list* list, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods, const char* command);