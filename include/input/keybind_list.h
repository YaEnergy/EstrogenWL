#pragma once

#include "input/keybind.h"

struct e_keybind_list
{
    struct e_keybind* keybinds;
    int amount;
};

struct e_keybind_list e_keybind_list_create();

int e_keybind_list_add(struct e_keybind_list* list, struct e_keybind keybind);

struct e_keybind e_keybind_list_at(struct e_keybind_list* list, int index);

void e_keybind_list_destroy(struct e_keybind_list* list);