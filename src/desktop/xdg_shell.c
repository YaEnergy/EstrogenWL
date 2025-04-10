#include "desktop/xdg_shell.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>

#include "util/log.h"

#include "desktop/views/toplevel_view.h"

static void e_xdg_shell_new_toplevel_view(struct wl_listener* listener, void* data)
{
    struct e_xdg_shell* shell = wl_container_of(listener, shell, new_toplevel_view);

    struct wlr_xdg_toplevel* xdg_toplevel = data;

    e_log_info("New toplevel view");

    //creates a top level view for this shell's desktop
    e_toplevel_view_create(shell->desktop, xdg_toplevel);
}

static void e_xdg_shell_destroy(struct wl_listener* listener, void* data)
{
    struct e_xdg_shell* shell = wl_container_of(listener, shell, destroy);
    
    wl_list_remove(&shell->new_toplevel_view.link);
    wl_list_remove(&shell->destroy.link);

    free(shell);
}

struct e_xdg_shell* e_xdg_shell_create(struct wl_display* display, struct e_desktop* desktop)
{
    assert(display && desktop);
    
    struct e_xdg_shell* shell = calloc(1, sizeof(*shell));

    if (shell == NULL)
    {
        e_log_error("failed to allocate xdg_shell");
        return NULL;
    }

    //give the shell a pointer to the desktop
    shell->desktop = desktop;

    //create xdg shell v6 for this display
    shell->xdg_shell = wlr_xdg_shell_create(display, 6);

    //events

    //listen for new top level views
    shell->new_toplevel_view.notify = e_xdg_shell_new_toplevel_view;
    wl_signal_add(&shell->xdg_shell->events.new_toplevel, &shell->new_toplevel_view);

    shell->destroy.notify = e_xdg_shell_destroy;
    wl_signal_add(&shell->xdg_shell->events.destroy, &shell->destroy);

    return shell;
}