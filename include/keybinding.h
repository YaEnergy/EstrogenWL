#pragma once

// Keybinding functions

#include <xkbcommon/xkbcommon.h>

#include "types/keybind_list.h"

int e_keybinding_bind(struct e_keybind_list* list, xkb_keysym_t keysym, xkb_mod_mask_t mods, const char* command);