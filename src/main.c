#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>

#include "types/keybind_list.h"
#include "types/keyboard.h"
#include "types/server.h"
#include "keybinding.h"
#include "log.h"

// Entry point program
int main()
{
    wlr_log_init(WLR_DEBUG, NULL);
    printf("Welcome to EstrogenCompositor!\n");

    if (e_log_init() != 0)
    {
        printf("Log init failed...\n");

        return 1;
    }

    //TODO: wayland server
    struct e_server server = {0};

    if (e_server_init(&server) != 0)
    {
        e_log_error("failed to init server");
        return 1;
    }

    // test keybinds

    struct e_keybind_list keybind_list = e_keybind_list_new();

    //check out: xkbcommon.org
    //Important function: xkb_keysym_from_name (const char *name, enum xkb_keysym_flags flags)
    
    e_keybinding_bind(&keybind_list, XKB_KEY_F1, WLR_MODIFIER_ALT, "exec rofi -modi drun,run -show drun");
    e_keybinding_bind(&keybind_list, XKB_KEY_F2, WLR_MODIFIER_ALT, "exec alacritty");
    e_keybinding_bind(&keybind_list, XKB_KEY_F3, WLR_MODIFIER_ALT, "exit");
    e_keybinding_bind(&keybind_list, XKB_KEY_F4, WLR_MODIFIER_ALT, "kill");

    // add unix socket to wl display
    const char* socket = wl_display_add_socket_auto(server.display);
    if (!socket)
    {
        e_log_error("failed to create socket");
        wlr_backend_destroy(server.backend);
        return 1;
    }

    //set WAYLAND_DISPLAY env var
    setenv("WAYLAND_DISPLAY", socket, true);

    //running
    e_log_info("starting backend");
    if (!wlr_backend_start(server.backend))
    {
        e_log_error("failed to start backend");
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.display);
        return 1;
    }

    e_log_info("running wl display on WAYLAND_DISPLAY=%s", socket);
    wl_display_run(server.display);

    // display runs

    //destroy everything

    e_keybind_list_destroy(&keybind_list);

    wl_display_destroy_clients(server.display);

    wlr_allocator_destroy(server.allocator);
    wlr_renderer_destroy(server.renderer);
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.display);

    e_log_deinit();
    
    return 0;
}
