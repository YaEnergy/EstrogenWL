#include "desktop/xwayland.h"

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_compositor.h>

#include <wlr/xwayland.h>

#include "desktop/windows/xwayland_window.h"

#include "util/log.h"

void e_xwayland_new_surface(struct wl_listener* listener, void* data)
{
    struct e_xwayland* xwayland = wl_container_of(listener, xwayland, new_surface);
    struct wlr_xwayland_surface* wlr_xwayland_surface = data;

    e_log_info("new xwayland surface");

    e_xwayland_window_create(xwayland->server, wlr_xwayland_surface);
}

struct e_xwayland* e_xwayland_create(struct e_server* server, struct wl_display* display, struct wlr_compositor* compositor, struct wlr_seat* seat)
{
    struct e_xwayland* xwayland = calloc(1, sizeof(struct e_xwayland));
    xwayland->server = server;
    xwayland->wlr_xwayland = wlr_xwayland_create(display, compositor, false);

    wlr_xwayland_set_seat(xwayland->wlr_xwayland, seat);

    //events

    //listen for new xwayland surfaces and stuff
    
    xwayland->new_surface.notify = e_xwayland_new_surface;
    wl_signal_add(&xwayland->wlr_xwayland->events.new_surface, &xwayland->new_surface);

    return xwayland;
}

void e_xwayland_destroy(struct e_xwayland* xwayland)
{
    wl_list_remove(&xwayland->new_surface.link);

    wlr_xwayland_destroy(xwayland->wlr_xwayland);
}