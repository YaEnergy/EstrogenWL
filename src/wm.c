#include "wm.h"

#include <wayland-util.h>

#include "server.h"
#include "output.h"
#include "desktop/windows/window.h"

void e_tile_windows(struct e_server *server)
{
    // gets width & height of first output
    //TODO: tiling doesn't work well with multiple outputs

    struct e_output* output = e_server_get_output(server, 0);

    if (output == NULL)
        return;

    int width = output->wlr_output->width;
    int height = output->wlr_output->height;

    //position and resize all windows

    if (wl_list_empty(&server->windows))
        return;

    int length = wl_list_length(&server->windows);
    int i = 0;
    struct e_window* window;

    wl_list_for_each(window, &server->windows, link)
    {
        e_window_set_position(window, i * (width / length), 0);
        e_window_set_size(window, width / length, height);
        i++;
    }
}