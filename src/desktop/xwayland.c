#include "desktop/xwayland.h"

#include <stdint.h>
#include <stdlib.h>
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

#include "util/log.h"
#include "util/wl_macros.h"

static void e_xwayland_new_surface(struct wl_listener* listener, void* data)
{
    struct e_xwayland* xwayland = wl_container_of(listener, xwayland, new_surface);
    struct wlr_xwayland_surface* xwayland_surface = data;

    if (xwayland_surface->override_redirect)
    {
        struct e_xwayland_unmanaged* unmanaged = e_xwayland_unmanaged_create(xwayland->desktop, xwayland_surface);

        if (unmanaged != NULL)
            e_log_info("new xwayland unmanaged surface!");
        else
            e_log_error("e_xwayland_new_surface: failed to create xwayland unmanaged surface");
    }
    else
    {
        struct e_xwayland_view* view = e_xwayland_view_create(xwayland->desktop, xwayland_surface);
        
        if (view != NULL)
            e_log_info("new xwayland view!");
        else
            e_log_error("e_xwayland_new_surface: failed to create xwayland view");
    }
}

// XCB connection is valid.
static void e_xwayland_ready(struct wl_listener* listener, void* data)
{
    struct e_xwayland* xwayland = wl_container_of(listener, xwayland, ready);

    e_log_info("xwayland is ready!");

    e_xwayland_update_workarea(xwayland);
}

// Creates a struct handling xwayland shell v1 protocol, server and X11 wm.
// Returns NULL on fail.
struct e_xwayland* e_xwayland_create(struct e_desktop* desktop, struct wl_display* display, struct wlr_compositor* compositor, struct wlr_seat* seat, bool lazy)
{
    assert(desktop && display && compositor && seat);

    struct e_xwayland* xwayland = calloc(1, sizeof(*xwayland));

    if (xwayland == NULL)
    {
        e_log_error("failed to allocate xwayland");
        return NULL;
    }

    xwayland->desktop = desktop;
    xwayland->wlr_xwayland = wlr_xwayland_create(display, compositor, lazy);

    //wlroots sets seat of connection when ready
    wlr_xwayland_set_seat(xwayland->wlr_xwayland, seat);

    //events

    SIGNAL_CONNECT(xwayland->wlr_xwayland->events.ready, xwayland->ready, e_xwayland_ready);
    SIGNAL_CONNECT(xwayland->wlr_xwayland->events.new_surface, xwayland->new_surface, e_xwayland_new_surface);

    return xwayland;
}

// Update useable geometry not covered by panels, docks, etc.
void e_xwayland_update_workarea(struct e_xwayland* xwayland)
{
    assert(xwayland);

    //connection is not ready yet
    if (xwayland->wlr_xwayland->xwm == NULL)
        return;
    
    if (wl_list_empty(&xwayland->desktop->outputs))
        return;

    //TODO: use useable area for workarea only

    struct wlr_output_layout* layout = xwayland->desktop->output_layout;

    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;

    struct e_output* output = NULL;
    wl_list_for_each(output, &xwayland->desktop->outputs, link)
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
    wlr_xwayland_set_workareas(xwayland->wlr_xwayland, &workarea, 1);
}

void e_xwayland_destroy(struct e_xwayland* xwayland)
{
    assert(xwayland);

    SIGNAL_DISCONNECT(xwayland->ready);
    SIGNAL_DISCONNECT(xwayland->new_surface);

    wlr_xwayland_destroy(xwayland->wlr_xwayland);

    free(xwayland);
}
