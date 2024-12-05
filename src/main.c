#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "types/server.h"
#include "keybinding.h"
#include "log.h"
//#include "wm.h"

struct e_server* server = NULL;

static void e_at_exit()
{
    e_log_info("Exiting EstrogenCompositor");

    e_log_info("Keybinds deinit...");

    e_keybinding_deinit();

    e_log_info("Freeing managed windows...");

    //EWMFree();

    //Close server

    e_log_deinit();
}

// Entry point program
int main()
{
    printf("Welcome to EstrogenCompositor!\n");

    if (e_log_init() != 0)
    {
        printf("Log init failed...\n");

        return 1;
    }

    //TODO: wayland server

    //Initialize keybinding, if fails exit
    if (e_keybinding_init() != 0)
    {
        e_log_error("Keybinding init failed");
        
        exit(1);
        return 1;
    }

    atexit(e_at_exit);
    
    // test keybinds

    /*EKeybindingBind(display, XKeysymToKeycode(display, XStringToKeysym("F1")), Mod1Mask, "exec rofi -modi drun,run -show drun");
    EKeybindingBind(display, XKeysymToKeycode(display, XStringToKeysym("F2")), Mod1Mask, "exec alacritty");
    
    EKeybindingBind(display, XKeysymToKeycode(display, XStringToKeysym("F4")), Mod1Mask, "exit");

    EKeybindingBind(display, XKeysymToKeycode(display, XStringToKeysym("F5")), Mod1Mask, "kill");*/

    /// unreachable, but just in case ///
    exit(0);
    
    return 0;
}
