#include "desktop/xdg_shell.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <wlr/types/wlr_xdg_shell.h>

#include "desktop/tree/container.h"

#include "util/log.h"
#include "util/wl_macros.h"

#include "server.h"

static void xdg_shell_new_toplevel(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, new_toplevel);
    struct wlr_xdg_toplevel* xdg_toplevel = data;

    e_log_info("New toplevel view");

    struct e_toplevel_view* view = e_toplevel_view_create(xdg_toplevel, server->pending);

    if (view == NULL)
    {
        e_log_error("xdg_shell_new_toplevel: failed to create toplevel view!");
        wl_resource_post_no_memory(xdg_toplevel->resource);
        return;
    }

    struct e_view_container* view_container = e_view_container_create(server, &view->base);

    if (view_container == NULL)
    {
        e_log_error("xdg_shell_new_toplevel: failed to create view container for toplevel view!");
        wl_resource_post_no_memory(xdg_toplevel->resource);
        //TODO: destroy view?
        return;
    }
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
