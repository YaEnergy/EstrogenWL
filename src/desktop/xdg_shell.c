#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>

#include "util/log.h"

#include "desktop/xdg_shell.h"
#include "desktop/windows/toplevel_window.h"

static void e_xdg_shell_new_toplevel_window(struct wl_listener* listener, void* data)
{
    struct e_xdg_shell* shell = wl_container_of(listener, shell, new_toplevel_window);

    struct wlr_xdg_toplevel* xdg_toplevel = data;

    e_log_info("New toplevel window");

    //creates a top level window for this shell's desktop
    e_toplevel_window_create(shell->desktop, xdg_toplevel);
}

static void e_xdg_shell_destroy(struct wl_listener* listener, void* data)
{
    struct e_xdg_shell* shell = wl_container_of(listener, shell, destroy);
    
    wl_list_remove(&shell->new_toplevel_window.link);
    wl_list_remove(&shell->destroy.link);

    free(shell);
}

struct e_xdg_shell* e_xdg_shell_create(struct wl_display* display, struct e_desktop* desktop)
{
    struct e_xdg_shell* shell = calloc(1, sizeof(*shell));

    //give the shell a pointer to the desktop
    shell->desktop = desktop;

    //create xdg shell v6 for this display
    shell->xdg_shell = wlr_xdg_shell_create(display, 6);

    //events

    //listen for new top level windows
    shell->new_toplevel_window.notify = e_xdg_shell_new_toplevel_window;
    wl_signal_add(&shell->xdg_shell->events.new_toplevel, &shell->new_toplevel_window);

    shell->destroy.notify = e_xdg_shell_destroy;
    wl_signal_add(&shell->xdg_shell->events.destroy, &shell->destroy);

    return shell;
}