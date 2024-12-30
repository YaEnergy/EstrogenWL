#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>

#include <xkbcommon/xkbcommon-keysyms.h>

#include "server.h"

#include "input/keybinding.h"
#include "util/log.h"

// Entry point program
int main()
{
    wlr_log_init(WLR_DEBUG, NULL);
    printf("Welcome to EstrogenWL!\n");

    if (e_log_init() != 0)
    {
        printf("Log init failed...\n");

        return 1;
    }
    
    struct e_server server = {0};

    if (e_server_init(&server) != 0)
    {
        e_log_error("failed to init server");
        return 1;
    }

    // test keybinds
    
    //check out: xkbcommon.org
    //Important function: xkb_keysym_from_name (const char *name, enum xkb_keysym_flags flags)
    
    e_keybinding_bind(&server.input_manager->keybind_list, XKB_KEY_F1, WLR_MODIFIER_ALT, "exec rofi -modi drun,run -show drun");
    e_keybinding_bind(&server.input_manager->keybind_list, XKB_KEY_F2, WLR_MODIFIER_ALT, "exec alacritty");
    e_keybinding_bind(&server.input_manager->keybind_list, XKB_KEY_F3, WLR_MODIFIER_ALT, "exit");
    e_keybinding_bind(&server.input_manager->keybind_list, XKB_KEY_F4, WLR_MODIFIER_ALT, "kill");
    e_keybinding_bind(&server.input_manager->keybind_list, XKB_KEY_F5, WLR_MODIFIER_ALT, "toggle_fullscreen");
    e_keybinding_bind(&server.input_manager->keybind_list, XKB_KEY_F6, WLR_MODIFIER_ALT, "toggle_tiling");

    e_server_run(&server);

    // display runs

    //destroy everything
    
    e_server_destroy(&server);

    e_log_deinit();
    
    return 0;
}
