#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>

#include "types/keybind_list.h"
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

    //handles accepting clients from Unix socket, managing wl globals, ...
    e_log_info("creating display...");
    server.display = wl_display_create();
     if (server.display == NULL)
    {
        e_log_error("failed to create display");
        return 1;
    }

    //backend handles input and output hardware, autocreate automatically creates the backend we want
    e_log_info("creating backend...");
    server.backend = wlr_backend_autocreate(wl_display_get_event_loop(server.display), NULL);
    if (server.backend == NULL)
    {
        e_log_error("failed to create backend");
        return 1;
    }

    //TODO: output notify

    //renderer handles rendering
    e_log_info("creating renderer....");
    server.renderer = wlr_renderer_autocreate(server.backend);
    if (server.renderer == NULL)
    {
        e_log_error("failed to create renderer");
        return 1;
    }

    wlr_renderer_init_wl_display(server.renderer, server.display);

    // test keybinds

    struct e_keybind_list keybind_list = e_keybind_list_new();

    /*EKeybindingBind(display, XKeysymToKeycode(display, XStringToKeysym("F1")), Mod1Mask, "exec rofi -modi drun,run -show drun");
    EKeybindingBind(display, XKeysymToKeycode(display, XStringToKeysym("F2")), Mod1Mask, "exec alacritty");
    
    EKeybindingBind(display, XKeysymToKeycode(display, XStringToKeysym("F4")), Mod1Mask, "exit");

    EKeybindingBind(display, XKeysymToKeycode(display, XStringToKeysym("F5")), Mod1Mask, "kill");*/

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

    wlr_renderer_destroy(server.renderer);
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.display);

    e_log_deinit();
    
    return 0;
}
