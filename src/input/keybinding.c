#include "input/keybinding.h"

#include <stdbool.h>
#include <stdbool.h>

#include <xkbcommon/xkbcommon.h>

#include "input/keybind.h"
#include "util/list.h"

bool e_keybinding_bind(struct e_list* keybinds, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods, const char* command)
{
    struct e_keybind* keybind = e_keybind_create(keysym, mods, command);

    if (keybind == NULL)
        return false;

    return e_list_add(keybinds, keybind);
}