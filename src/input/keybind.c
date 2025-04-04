#include "input/keybind.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <xkbcommon/xkbcommon.h>

#include "util/log.h"

struct e_keybind* e_keybind_create(xkb_keysym_t keysym, enum wlr_keyboard_modifier mods, const char* command)
{
    struct e_keybind* keybind = calloc(1, sizeof(*keybind));

    if (keybind == NULL)
    {
        e_log_error("failed to allocate keybind");
        return NULL;
    }

    keybind->keysym = keysym;
    keybind->mods = mods;
    keybind->command = command;

    return keybind;
}

bool e_keybind_should_activate(struct e_keybind* keybind, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods)
{
    return (keybind->keysym == keysym && keybind->mods == mods);
}

void e_keybind_free(struct e_keybind* keybind)
{
    assert(keybind);

    free(keybind);
}