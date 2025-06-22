#include "desktop/xwayland.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/box.h>
#include <wlr/xwayland.h>

#include "input/seat.h"

#include "desktop/output.h"
#include "desktop/desktop.h"

#include "util/log.h"
#include "util/wl_macros.h"

#include "server.h"

static void xwayland_new_surface(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, new_xwayland_surface);
    struct wlr_xwayland_surface* xwayland_surface = data;

    if (xwayland_surface->override_redirect)
    {
        struct e_xwayland_unmanaged* unmanaged = e_xwayland_unmanaged_create(server, xwayland_surface);

        if (unmanaged != NULL)
            e_log_info("new xwayland unmanaged surface!");
        else
            e_log_error("xwayland_new_surface: failed to create xwayland unmanaged surface");
    }
    else
    {
        struct e_xwayland_view* view = e_xwayland_view_create(server, xwayland_surface);
        
        if (view != NULL)
            e_log_info("new xwayland view!");
        else
            e_log_error("xwayland_new_surface: failed to create xwayland view");
    }
}

// XCB connection is valid.
static void xwayland_ready(struct wl_listener* listener, void* data)
{
    struct e_server* server = wl_container_of(listener, server, xwayland_ready);

    e_log_info("xwayland is ready!");

    e_server_update_xwayland_workareas(server);
}

bool e_server_init_xwayland(struct e_server* server, struct e_seat* seat, bool lazy)
{
    assert(server && server->compositor && seat);

    if (server == NULL || server->compositor == NULL || seat == NULL)
        return false;

    server->xwayland = wlr_xwayland_create(server->display, server->compositor, lazy);

    if (server->xwayland == NULL)
    {
        e_log_error("e_server_init_xwayland: failed to create wlr_xwayland");
        return false;
    }

    //wlroots sets seat of connection when ready
    wlr_xwayland_set_seat(server->xwayland, seat->wlr_seat);

    //events

    SIGNAL_CONNECT(server->xwayland->events.ready, server->xwayland_ready, xwayland_ready);
    SIGNAL_CONNECT(server->xwayland->events.new_surface, server->new_xwayland_surface, xwayland_new_surface);

    return true;
}

void e_server_fini_xwayland(struct e_server* server)
{
    assert(server);

    if (server == NULL)
        return;

    SIGNAL_DISCONNECT(server->xwayland_ready);
    SIGNAL_DISCONNECT(server->new_xwayland_surface);

    wlr_xwayland_destroy(server->xwayland);
}

// Update useable geometry not covered by panels, docks, etc. for xwayland
void e_server_update_xwayland_workareas(struct e_server* server)
{
    assert(server && server->xwayland);

    if (server == NULL || server->xwayland == NULL)
        return;

    struct wlr_xwayland* xwayland = server->xwayland;

    //connection is not ready yet
    if (xwayland->xwm == NULL)
        return;
    
    if (wl_list_empty(&server->outputs))
        return;

    //TODO: set separate workareas for each output
    //TODO: use useable area for workarea only

    struct wlr_output_layout* layout = server->output_layout;

    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;

    struct e_output* output = NULL;
    wl_list_for_each(output, &server->outputs, link)
    {
        struct wlr_box output_box;
        wlr_output_layout_get_box(layout, output->wlr_output, &output_box);

        if (output_box.x < left)
            left = output_box.x;

        if (output_box.x + output_box.width > right)
            right = output_box.x + output_box.width;

        if (output_box.y < top)
            top = output_box.y;

        if (output_box.y + output_box.height > bottom)
            bottom = output_box.y + output_box.height;
    }

    struct wlr_box workarea = {left, top, right - left, bottom - top};
    wlr_xwayland_set_workareas(xwayland, &workarea, 1);
}
