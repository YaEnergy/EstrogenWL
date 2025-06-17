#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_keyboard.h>

#include <xkbcommon/xkbcommon-keysyms.h>

#include "session.h"
#include "config.h"
#include "server.h"

#include "input/keybind.h"
#include "util/log.h"

static bool bind_keybind(struct e_list* keybinds, xkb_keysym_t keysym, enum wlr_keyboard_modifier mods, const char* command)
{
    struct e_keybind* keybind = e_keybind_create(keysym, mods, command);

    if (keybind == NULL)
        return false;

    return e_list_add(keybinds, keybind);
}

// Called when event loop is ready.
static void event_loop_handle_ready(void* data)
{
    e_log_info("event loop is ready!");

    e_log_info("running autostart.sh script");

    if (!e_session_autostart_run())
        e_log_error("event_loop_ready: failed to run autostart.sh");

    /* idle events are automatically removed when they're done */
}

// Entry point program
int main()
{
    e_log_init();

    e_log_info("Welcome to EstrogenWL!");

    e_session_init_env();

    struct e_config config = {0};
    e_config_init(&config);

    // test keybinds
    
    //check out: xkbcommon.org
    //Important function: xkb_keysym_from_name (const char *name, enum xkb_keysym_flags flags)
    
    bind_keybind(&config.keyboard.keybinds, XKB_KEY_F1, WLR_MODIFIER_LOGO, "exec rofi -modi drun,run -show drun");
    bind_keybind(&config.keyboard.keybinds, XKB_KEY_F2, WLR_MODIFIER_LOGO, "exec $TERM");
    bind_keybind(&config.keyboard.keybinds, XKB_KEY_F3, WLR_MODIFIER_LOGO, "exit");
    bind_keybind(&config.keyboard.keybinds, XKB_KEY_F4, WLR_MODIFIER_LOGO, "kill");
    bind_keybind(&config.keyboard.keybinds, XKB_KEY_F5, WLR_MODIFIER_LOGO, "toggle_fullscreen");
    bind_keybind(&config.keyboard.keybinds, XKB_KEY_F6, WLR_MODIFIER_LOGO, "toggle_tiling");
    bind_keybind(&config.keyboard.keybinds, XKB_KEY_F7, WLR_MODIFIER_LOGO, "switch_tiling_mode");
    bind_keybind(&config.keyboard.keybinds, XKB_KEY_F8, WLR_MODIFIER_LOGO, "next_workspace");
    bind_keybind(&config.keyboard.keybinds, XKB_KEY_F9, WLR_MODIFIER_LOGO, "move_to_next_workspace");
    
    struct e_server server = {0};

    if (e_server_init(&server, &config) != 0)
    {
        e_log_error("failed to init server");
        return 1;
    }

    e_server_start(&server);

    //run autostart script when event loop is ready, removed automatically when dispatched
    //so when event loop is ready
    if (server.event_loop == NULL || wl_event_loop_add_idle(server.event_loop, event_loop_handle_ready, NULL) == NULL)
        e_log_error("main: failed to add autostart.sh event");

    e_server_run(&server);

    // display runs

    // destroy everything
    
    e_server_fini(&server);

    e_config_fini(&config);
    
    return 0;
}
