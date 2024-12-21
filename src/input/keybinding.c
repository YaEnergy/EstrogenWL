#include "input/keybinding.h"

#include <stdbool.h>

#include <xkbcommon/xkbcommon.h>

#include "commands.h"

#include "input/keybind.h"
#include "input/keybind_list.h"
#include "server.h"

int e_keybinding_bind(struct e_keybind_list* list, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods, const char* command)
{
    struct e_keybind keybind = {keysym, mods, command};
    return e_keybind_list_add(list, keybind);
}

bool e_keybinding_handle(struct e_server* server, struct e_keybind_list* keybinds, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods)
{
    for (int i = 0; i < keybinds->amount; i++)
    {
        struct e_keybind keybind = e_keybind_list_at(keybinds, i);

        if (e_keybind_should_activate(keybind, keysym, mods))
        {
            e_commands_parse(server, keybind.command);
            return true;
        }
    }

    return false;
}