#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <xkbcommon/xkbcommon.h>

#include <wlr/types/wlr_keyboard.h>

struct e_keybind
{
    xkb_keysym_t keysym;
    uint32_t mods; //bit flags enum wlr_keyboard_modifier
    char* command;
};

// Command is copied.
// mods is enum wlr_keyboard_modifier bit flags
// Returns NULL on fail.
struct e_keybind* e_keybind_create(xkb_keysym_t keysym, uint32_t mods, const char* command);

// mods is enum wlr_keyboard_modifier bit flags
bool e_keybind_should_activate(struct e_keybind* keybind, xkb_keysym_t keysym, uint32_t mods);

void e_keybind_free(struct e_keybind* keybind);
