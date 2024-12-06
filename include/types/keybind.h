#pragma once

#include <stdbool.h>

#include <xkbcommon/xkbcommon.h>

struct e_keybind
{
    xkb_keysym_t keysym;
    xkb_mod_mask_t mods;
    const char* command;
};

bool e_keybind_equals(struct e_keybind a, struct e_keybind b);

bool e_keybind_should_activate(struct e_keybind keybind, xkb_keysym_t keysym, xkb_mod_mask_t mods);