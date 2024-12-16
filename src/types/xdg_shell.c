#include "wayland-server-core.h"

#include "types/server.h"
#include <stdlib.h>
#include <wayland-util.h>
#include "types/xdg_shell.h"


static void e_xdg_shell_new_toplevel_window(struct wl_listener* listener, void* data)
{
    //TODO: implement e_xdg_shell_new_toplevel_window
}

static void e_xdg_shell_new_popup_window(struct wl_listener* listener, void* data)
{
    //TODO: implement e_xdg_shell_new_popup_window
}

struct e_xdg_shell* e_xdg_shell_create(struct e_server* server)
{
    struct e_xdg_shell* shell = calloc(1, sizeof(struct e_xdg_shell));

    //give the shell a pointer to the server
    shell->server = server;

    //create xdg shell v6 for this server's display
    shell->xdg_shell = wlr_xdg_shell_create(server->display, 6);

    //init top level windows list
    wl_list_init(&shell->toplevel_windows);

    //listen for new top level windows
    shell->new_toplevel_window.notify = e_xdg_shell_new_toplevel_window;
    wl_signal_add(&shell->xdg_shell->events.new_toplevel, &shell->new_toplevel_window);

    //listen for new pop up windows
    shell->new_popup_window.notify = e_xdg_shell_new_popup_window;
    wl_signal_add(&shell->xdg_shell->events.new_popup, &shell->new_popup_window);

    //set server's xdg shell
    server->xdg_shell = shell;

    return shell;
}

void e_xdg_shell_destroy(struct e_xdg_shell* shell)
{
    wl_list_remove(&shell->toplevel_windows);

    wl_list_remove(&shell->new_toplevel_window.link);
    wl_list_remove(&shell->new_popup_window.link);

    free(shell);
}