#include "desktop/desktop.h"

#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include "input/seat.h"
#include "input/cursor.h"

#include "desktop/tree/container.h"
#include "desktop/layer_shell.h"
#include "desktop/output.h"

#include "server.h"

/* hover */

// Returns output currently hovered by current seat's cursor.
// Returns NULL if no output is being hovered.
struct e_output* e_desktop_hovered_output(struct e_server* server)
{
    assert(server);

    return e_cursor_hovered_output(server->seat->cursor);
}

// Returns container currently hovered by current seat's cursor.
// Returns NULL if no container is being hovered.
struct e_container* e_desktop_hovered_container(struct e_server* server)
{
    assert(server);

    return e_cursor_hovered_container(server->seat->cursor);
}

/* focus */

void e_desktop_set_focus_view_container(struct e_server* server, struct e_view_container* view_container)
{
    assert(server);

    e_seat_set_focus_view_container(server->seat, view_container);
}

void e_desktop_set_focus_layer_surface(struct e_server* server, struct e_layer_surface* layer_surface)
{
    assert(server);

    e_seat_set_focus_layer_surface(server->seat, layer_surface);
}

// Returns view container currently in focus.
// Returns NULL if no view container has focus.
struct e_view_container* e_desktop_focused_view_container(struct e_server* server)
{
    assert(server);

    return server->seat->focus.active_view_container;
}
