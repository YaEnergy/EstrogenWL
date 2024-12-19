#pragma once

#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

#include <wlr/types/wlr_keyboard.h>

#include "types/input/keybind.h"

struct e_keybind_list
{
    struct e_keybind* keybinds;
    int amount;
};

struct e_keybind_list e_keybind_list_new();

int e_keybind_list_add(struct e_keybind_list* list, struct e_keybind keybind);

//returns index of keybind with exact same keysym, mod mask, AND COMMAND, returns -1 if not found
int e_keybind_list_index_of(struct e_keybind_list list, struct e_keybind keybind);

//returns index of keybind that should activate, returns -1 if not found
int e_keybind_list_should_activate_index_of(struct e_keybind_list list, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods);

struct e_keybind e_keybind_list_at(struct e_keybind_list list, int index);

void e_keybind_list_destroy(struct e_keybind_list* list);