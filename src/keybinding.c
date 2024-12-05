#include "keybinding.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

#include "commands.h"
#include "log.h"

struct e_keybind
{
    xkb_keysym_t keycode;
    xkb_mod_mask_t mods;
    const char* command;
};

int keybindCount = 0;
int keybindCapacity = 5;
struct e_keybind* keybinds = NULL;

static bool keybind_does_input_activate(struct e_keybind keybind, xkb_keysym_t keycode, xkb_mod_mask_t mods)
{
    return (keybind.keycode == keycode && keybind.mods == mods);
}

static bool keybind_exists(xkb_keysym_t keycode, xkb_mod_mask_t mods)
{
    /*for (int i = 0; i < keybindCount; i++)
    {
        EKeybind keybind = keybinds[i];
        
        if (doesInputActivateKeybind(keybind, display, keyCode, modifierMask))
        {
            return true;
        }
    }*/

    //TODO: implement

    return false;
}

static int expandKeybindArray(int capacityAmount)
{
    //allocate new array with increased capacity with keybinds
    struct e_keybind* newKeybinds = realloc(keybinds, sizeof(struct e_keybind) * (keybindCapacity + capacityAmount));
    
    if (newKeybinds == NULL)
    {
        //allocation fail
        e_log_error("Failed to expand keybind array, couldn't allocate new array");
        //don't free the original keybind array, it should still be usable
        return 1;
    }

    e_log_info("Allocated keybind array for %d keybinds max", keybindCapacity + capacityAmount);

    //point to new keybind array
    keybinds = newKeybinds;

    keybindCapacity += capacityAmount;

    e_log_info("Expanded keybind array to %d keybinds max", keybindCapacity);

    return 0;
}

int e_keybinding_init()
{
    //allocate memory for keybinds
    keybinds = malloc(sizeof(struct e_keybind) * keybindCapacity);

    if (keybinds == NULL)
    {
        e_log_error("Failed to allocate keybind array");
        return 1;
    }
    else
    {
        e_log_info("Allocated keybind array for %d keybinds max", keybindCapacity);
    }

    return 0;
}

void e_keybinding_deinit()
{
    if (keybinds != NULL)
        free(keybinds);
}

int e_keybinding_bind(xkb_keysym_t keycode, xkb_mod_mask_t mods, const char *command)
{
    //if keybind array full, expand it for new keybind
    if (keybindCount >= keybindCapacity)
    {
        if (expandKeybindArray(1) != 0)
            return -1;
    }
    
    /*
    //if duplicate, don't add
    if (doesKeybindExist(display, keyCode, modifierMask))
        return -2;

    //Get x events for this key with this mask
    XGrabKey(display, keyCode, modifierMask, DefaultRootWindow(display), True, GrabModeAsync, GrabModeAsync);

    EKeybind keybind = {display, keyCode, modifierMask, command};

    keybinds[keybindCount] = keybind;
    keybindCount++;

    e_log_info("Bound key code %u (with modifier mask: %i) to command: %s", (unsigned int)keyCode, modifierMask, command);
    */
    //TODO: implement

    return keybindCount - 1;
}

/*void EKeybindingHandleInput(Display* display, KeyCode keyCode, int modifierMask)
{
    //search for first matching keybind, and execute its command in shell
    for (int i = 0; i < keybindCount; i++)
    {
        EKeybind keybind = keybinds[i];
        
        if (doesInputActivateKeybind(keybind, display, keyCode, modifierMask))
        {
            ECommandsParse(keybind.command, display);
            return;
        }
    }
}
*/