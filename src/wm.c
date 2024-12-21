#include "wm.h"

#include <wayland-util.h>

#include "server.h"
#include "output.h"
#include "windows/toplevel_window.h"

void e_tile_toplevel_windows(struct e_server *server)
{
    // gets width & height of last output
    //TODO: tiling doesn't work well with multiple outputs

    int width = 0;
    int height = 0;

    //TODO: find a better way to get output width and height of specific outputs

    struct e_output* output;

    if (wl_list_empty(&server->outputs))
        return;

    wl_list_for_each(output, &server->outputs, link)
    {
        width = output->wlr_output->width;
        height = output->wlr_output->height;
    }

    //position and resize all toplevel windows

    if (wl_list_empty(&server->xdg_shell->toplevel_windows))
        return;

    int length = wl_list_length(&server->xdg_shell->toplevel_windows);
    int i = 0;
    struct e_toplevel_window* toplevel_window;

    wl_list_for_each(toplevel_window, &server->xdg_shell->toplevel_windows, link)
    {
        e_toplevel_window_set_position(toplevel_window, i * (width / length), 0);
        e_toplevel_window_set_size(toplevel_window, width / length, height);
        i++;
    }
}