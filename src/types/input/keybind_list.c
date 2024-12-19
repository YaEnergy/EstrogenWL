#include <stdbool.h>
#include <stdlib.h>

#include "types/input/keybind.h"
#include "types/input/keybind_list.h"

#include "log.h"

struct e_keybind_list e_keybind_list_new()
{
    struct e_keybind_list keybind_list = {NULL, 0};
    return keybind_list;
}

int e_keybind_list_add(struct e_keybind_list* list, struct e_keybind keybind)
{
    //allocate new list with more space
    struct e_keybind* newKeybinds = NULL;

    if (list->amount == 0)
        newKeybinds = calloc(list->amount + 1, sizeof(struct e_keybind));
    else
        newKeybinds = realloc(list->keybinds, sizeof(struct e_keybind) * (list->amount + 1));
    
    if (newKeybinds == NULL)
    {
        //allocation fail
        e_log_error("Failed to expand keybind list, couldn't allocate new list");
        //don't free the original keybind list, it should still be usable
        return 1;
    }

    //point towards new keybind list
    list->keybinds = newKeybinds;

    list->keybinds[list->amount] = keybind;
    list->amount++;

    return 0;
}

int e_keybind_list_index_of(struct e_keybind_list list, struct e_keybind keybind)
{
    for (int i = 0; i < list.amount; i++)
    {
        if (e_keybind_equals(list.keybinds[i], keybind))
            return i;
    }

    return -1;
}

int e_keybind_list_should_activate_index_of(struct e_keybind_list list, xkb_keysym_t keysym, xkb_mod_mask_t mods)
{
    for (int i = 0; i < list.amount; i++)
    {
        if (e_keybind_should_activate(list.keybinds[i], keysym, mods))
            return i;
    }

    return -1;
}

struct e_keybind e_keybind_list_at(struct e_keybind_list list, int index)
{
    //index must be inside list
    if (index < 0 || index >= list.amount)
    {
        struct e_keybind nullbind = {};
        return nullbind;
    }

    return list.keybinds[index];
}

void e_keybind_list_destroy(struct e_keybind_list* list)
{
    if (list->keybinds != NULL)
        free(list->keybinds);
}