#pragma once

#include <stdbool.h>

#include <xkbcommon/xkbcommon.h>

typedef uint32_t e_wlr_modifier_mask;

struct e_keybind
{
    xkb_keysym_t keysym;
    e_wlr_modifier_mask mods;
    const char* command;
};

bool e_keybind_equals(struct e_keybind a, struct e_keybind b);

bool e_keybind_should_activate(struct e_keybind keybind, xkb_keysym_t keysym, e_wlr_modifier_mask mods);