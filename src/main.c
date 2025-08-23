#include <stdbool.h>

#include <wayland-server-core.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_keyboard.h>

#include <xkbcommon/xkbcommon-keysyms.h>

#include "session.h"
#include "config.h"
#include "server.h"

#include "util/log.h"

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

    //TODO: replace config path with user's specified path
    if (!e_config_parse(&config, "/home/kiara/.config/EstrogenWL/config.json"))
        return 1;
    
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
