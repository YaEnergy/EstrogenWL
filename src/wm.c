#include "wm.h"

#include <wayland-util.h>

#include "server.h"
#include "output.h"
#include "desktop/windows/window.h"
#include "desktop/scene.h"
#include "util/log.h"

void e_tile_windows(struct e_server *server)
{
    e_log_info("tiling windows...");
    
    // gets width & height of first output
    //TODO: tiling doesn't work well with multiple outputs

    struct e_output* output = e_scene_get_output(server->scene, 0);

    if (output == NULL)
        return;

    int width = output->wlr_output->width;
    int height = output->wlr_output->height;

    //position and resize all tiled windows

    if (wl_list_empty(&server->scene->windows))
        return;

    struct e_window* window;

    //get number of tiled windows
    int amountTiled = 0;

    wl_list_for_each(window, &server->scene->windows, link)
    {
        if (window->tiled)
            amountTiled++;
    }

    //tile tiled windows (very basic horizontal tiling)

    int i = 0;

    wl_list_for_each(window, &server->scene->windows, link)
    {
        if (!window->tiled)
            continue;

        e_window_set_position(window, i * (width / amountTiled), 0);
        e_window_set_size(window, width / amountTiled, height);
        i++;
    }
}