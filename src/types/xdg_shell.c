#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include "log.h"
#include "types/server.h"
#include "types/xdg_shell.h"
#include "types/windows/popup_window.h"
#include "types/windows/toplevel_window.h"

static void e_xdg_shell_new_toplevel_window(struct wl_listener* listener, void* data)
{
    struct e_xdg_shell* shell = wl_container_of(listener, shell, new_toplevel_window);

    struct wlr_xdg_toplevel* xdg_toplevel = data;

    e_log_info("New toplevel window");

    //creates a top level window for this shell's server
    e_toplevel_window_create(shell->server, xdg_toplevel);
}

static void e_xdg_shell_new_popup_window(struct wl_listener* listener, void* data)
{
    struct e_xdg_shell* shell = wl_container_of(listener, shell, new_popup_window);
    struct wlr_xdg_popup* xdg_popup = data;

    e_log_info("New popup window");

    //creates a popup window, will destroy itself when done
    e_popup_window_create(xdg_popup);
}

static void e_xdg_shell_destroy(struct wl_listener* listener, void* data)
{
    struct e_xdg_shell* shell = wl_container_of(listener, shell, destroy);

    wl_list_remove(&shell->toplevel_windows);

    wl_list_remove(&shell->new_toplevel_window.link);
    wl_list_remove(&shell->new_popup_window.link);
    wl_list_remove(&shell->destroy.link);

    free(shell);
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

    //events

    //listen for new top level windows
    shell->new_toplevel_window.notify = e_xdg_shell_new_toplevel_window;
    wl_signal_add(&shell->xdg_shell->events.new_toplevel, &shell->new_toplevel_window);

    //listen for new pop up windows
    shell->new_popup_window.notify = e_xdg_shell_new_popup_window;
    wl_signal_add(&shell->xdg_shell->events.new_popup, &shell->new_popup_window);

    shell->destroy.notify = e_xdg_shell_destroy;
    wl_signal_add(&shell->xdg_shell->events.destroy, &shell->destroy);

    //set server's xdg shell
    server->xdg_shell = shell;

    return shell;
}