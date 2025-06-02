#include "desktop/xdg_shell.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-protocol.h>

#include <wlr/types/wlr_xdg_shell.h>

#include "util/log.h"
#include "util/wl_macros.h"

#include "server.h"

static void xdg_shell_new_toplevel(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, new_toplevel);
    struct wlr_xdg_toplevel* xdg_toplevel = data;

    e_log_info("New toplevel view");

    //creates a top level view on the desktop
    e_toplevel_view_create(server->desktop, xdg_toplevel);
}

bool e_server_init_xdg_shell(struct e_server* server)
{
    assert(server);

    if (server == NULL)
        return false;

    server->xdg_shell = wlr_xdg_shell_create(server->display, E_XDG_WM_BASE_VERSION);

    if (server->xdg_shell == NULL)
    {
        e_log_error("init_xdg_shell: failed to create xdg shell!");
        return false;
    }

    SIGNAL_CONNECT(server->xdg_shell->events.new_toplevel, server->new_toplevel, xdg_shell_new_toplevel);

    return true;
}

void e_server_fini_xdg_shell(struct e_server* server)
{
    assert(server);

    if (server == NULL)
        return;

    SIGNAL_DISCONNECT(server->new_toplevel);
}
