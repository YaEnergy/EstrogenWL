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

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "desktop/views/xwayland_view.h"

#include "input/seat.h"

#include "output.h"

#include "util/log.h"

// All names for e_xcb_atom_type enum values
const char* atom_names[] = {
    [E_NET_WM_WINDOW_TYPE_DESKTOP] = "_NET_WM_WINDOW_TYPE_DESKTOP",
    [E_NET_WM_WINDOW_TYPE_DOCK] = "_NET_WM_WINDOW_TYPE_DOCK",
    [E_NET_WM_WINDOW_TYPE_TOOLBAR] = "_NET_WM_WINDOW_TYPE_TOOLBAR",
    [E_NET_WM_WINDOW_TYPE_MENU] = "_NET_WM_WINDOW_TYPE_MENU",
    [E_NET_WM_WINDOW_TYPE_UTILITY] = "_NET_WM_WINDOW_TYPE_UTILITY",
    [E_NET_WM_WINDOW_TYPE_SPLASH] = "_NET_WM_WINDOW_TYPE_SPLASH",
    [E_NET_WM_WINDOW_TYPE_DIALOG] = "_NET_WM_WINDOW_TYPE_DIALOG",
    [E_NET_WM_WINDOW_TYPE_DROPDOWN_MENU] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
    [E_NET_WM_WINDOW_TYPE_POPUP_MENU] = "_NET_WM_WINDOW_TYPE_POPUP_MENU",
    [E_NET_WM_WINDOW_TYPE_TOOLTIP] = "_NET_WM_WINDOW_TYPE_TOOLTIP",
    [E_NET_WM_WINDOW_TYPE_NOTIFICATION] = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
    [E_NET_WM_WINDOW_TYPE_COMBO] = "_NET_WM_WINDOW_TYPE_COMBO",
    [E_NET_WM_WINDOW_TYPE_DND] = "_NET_WM_WINDOW_TYPE_DND",
    [E_NET_WM_WINDOW_TYPE_NORMAL] = "_NET_WM_WINDOW_TYPE_NORMAL"
};

static void e_xwayland_new_surface(struct wl_listener* listener, void* data)
{
    struct e_xwayland* xwayland = wl_container_of(listener, xwayland, new_surface);
    struct wlr_xwayland_surface* wlr_xwayland_surface = data;

    e_log_info("new xwayland surface");

    if (!wlr_xwayland_surface->override_redirect)
        e_xwayland_view_create(xwayland->desktop, xwayland, wlr_xwayland_surface);
    else
        e_log_error("e_xwayland_new_surface: no support for unmanaged surfaces yet! (override redirect)");
}

// Xwayland connection is valid.
static void e_xwayland_ready(struct wl_listener* listener, void* data)
{
    struct e_xwayland* xwayland = wl_container_of(listener, xwayland, ready);

    e_log_info("xwayland is ready!");

    e_xwayland_update_workarea(xwayland);

    // Get atom ids we need, so we can check for certain window types. (Useful for e_view_xwayland_wants_floating).
    // TODO: it seems like wlroots will soon have a way of checking the window type in 0.19.0 https://wlroots.pages.freedesktop.org/wlroots/wlr/xwayland/xwayland.h.html#func-wlr_xwayland_surface_has_window_type, making this soon redundant.
    //thanks Sway

    xcb_connection_t* xcb_connection = wlr_xwayland_get_xwm_connection(xwayland->wlr_xwayland);

    if (xcb_connection == NULL)
    {
        e_log_error("e_xwayland_ready: what do you mean xcb connection is null this is the event where it became valid???");
        return;
    }

    int error_state = xcb_connection_has_error(xcb_connection);

    if (error_state > 0)
    {
        e_log_error("e_xwayland_ready: xcb connection is in an error state. (XCB: %i)", error_state);
        return;
    }
    
    //request all intern atom cookies for the atoms we need

    xcb_intern_atom_cookie_t cookies[E_ATOMS_LEN];

    for (int i = 0; i < E_ATOMS_LEN; i++)
        cookies[i] = xcb_intern_atom(xcb_connection, 0, (uint16_t)strlen(atom_names[i]), atom_names[i]);

    //reply to all cookies and extract the atom type

    for (int i = 0; i < E_ATOMS_LEN; i++)
    {
        xcb_generic_error_t* error = NULL;
        xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(xcb_connection, cookies[i], &error);

        if (reply != NULL)
        {
            if (error == NULL)
            {
                xwayland->atoms[i] = reply->atom;
                e_log_info("xwayland: resolved atom %s (ID: %zi)", atom_names[i], reply->atom);
            }

            free(reply);
        }

        if (error != NULL)
        {
            e_log_error("e_xwayland_ready: unable to resolve atom %s. (X11: %i)", atom_names[i], error->error_code);
            free(error);
        }
    }
}

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
    
    xwayland->ready.notify = e_xwayland_ready;
    wl_signal_add(&xwayland->wlr_xwayland->events.ready, &xwayland->ready);

    xwayland->new_surface.notify = e_xwayland_new_surface;
    wl_signal_add(&xwayland->wlr_xwayland->events.new_surface, &xwayland->new_surface);

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

    wl_list_remove(&xwayland->ready.link);
    wl_list_remove(&xwayland->new_surface.link);

    wlr_xwayland_destroy(xwayland->wlr_xwayland);

    free(xwayland);
}