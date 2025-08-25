#include "input/keybind.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <string.h>

#include <xkbcommon/xkbcommon.h>

#include "util/log.h"

struct e_keybind* e_keybind_create(xkb_keysym_t keysym, enum wlr_keyboard_modifier mods, const char* command)
{
    assert(command);

    struct e_keybind* keybind = calloc(1, sizeof(*keybind));

    if (keybind == NULL)
    {
        e_log_error("e_keybind_create: failed to allocate keybind");
        return NULL;
    }

    keybind->keysym = keysym;
    keybind->mods = mods;
    keybind->command = strdup(command);

    if (keybind->command == NULL)
    {
        e_log_error("e_keybind_create: failed to copy command string");
        free(keybind);
        return NULL;
    }

    return keybind;
}

bool e_keybind_should_activate(struct e_keybind* keybind, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods)
{
    return (xkb_keysym_to_lower(keybind->keysym) == xkb_keysym_to_lower(keysym) && keybind->mods == mods);
}

void e_keybind_free(struct e_keybind* keybind)
{
    assert(keybind);

    free(keybind->command);
    keybind->command = NULL;

    free(keybind);
}
