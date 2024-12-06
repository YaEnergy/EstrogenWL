#include "types/keybind.h"

#include <stdbool.h>

#include <string.h>
#include <xkbcommon/xkbcommon.h>

bool e_keybind_equals(struct e_keybind a, struct e_keybind b)
{
    return (a.keysym == b.keysym && a.mods == b.mods && strcmp(a.command, b.command) == 0);
}

bool e_keybind_should_activate(struct e_keybind keybind, xkb_keysym_t keysym, xkb_mod_mask_t mods)
{
    return (keybind.keysym == keysym && keybind.mods == mods);
}