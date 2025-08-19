#include "desktop/desktop.h"

#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "input/seat.h"
#include "input/cursor.h"

#include "desktop/views/view.h"
#include "desktop/tree/container.h"
#include "desktop/tree/workspace.h"
#include "desktop/layer_shell.h"
#include "desktop/output.h"

#include "util/log.h"

#include "server.h"

void e_desktop_state_init(struct e_desktop_state* desktop_state)
{
    assert(desktop_state);

    desktop_state->focused_layer_surface = NULL;
    desktop_state->active_view_container = NULL;
}

/* scene */

// Finds the scene surface at the specified layout coords in given scene graph.
// Also translates the layout coords to the surface coords if not NULL. (sx, sy)
// NULL for sx & sy is allowed.
// Returns NULL if nothing is found.
struct wlr_scene_surface* e_desktop_scene_surface_at(struct wlr_scene_node* node, double lx, double ly, double* sx, double* sy)
{
    assert(node);

    if (sx != NULL)
        *sx = 0.0;

    if (sy != NULL)
        *sy = 0.0;

    double nx, ny = 0.0;
    struct wlr_scene_node* snode = wlr_scene_node_at(node, lx, ly, &nx, &ny);

    if (snode == NULL || snode->type != WLR_SCENE_NODE_BUFFER)
        return NULL;

    struct wlr_scene_buffer* buffer = wlr_scene_buffer_from_node(snode);
    struct wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(buffer);

    if (scene_surface == NULL)
        return NULL;

    if (sx != NULL)
        *sx = nx;

    if (sy != NULL)
        *sy = ny;

    return scene_surface;
}

/* hover */

// Returns output currently hovered by cursor.
// Returns NULL if no output is being hovered.
struct e_output* e_desktop_hovered_output(struct e_server* server)
{
    assert(server);

    if (server == NULL)
    {
        e_log_error("e_desktop_hovered_output: server is NULL!");
        return NULL;
    }

    struct e_cursor* cursor = server->seat->cursor;

    struct wlr_output* wlr_output = wlr_output_layout_output_at(server->output_layout, cursor->wlr_cursor->x, cursor->wlr_cursor->y);
    
    if (wlr_output == NULL)
        return NULL;
    
    struct e_output* output = wlr_output->data;

    return output;
}

// Returns container currently hovered by cursor.
// Returns NULL if no container is being hovered.
struct e_container* e_desktop_hovered_container(struct e_server* server)
{
    assert(server);

    if (server == NULL)
    {
        e_log_error("e_desktop_hovered_output: server is NULL!");
        return NULL;
    }

    struct e_cursor* cursor = server->seat->cursor;

    return e_container_at(&server->scene->tree.node, cursor->wlr_cursor->x, cursor->wlr_cursor->y);
}

/* focus */

static bool desktop_has_exclusive_focus(struct e_server* server)
{
    assert(server);

    struct e_desktop_state* state = &server->desktop_state;

    return (state->focused_layer_surface != NULL && e_layer_surface_get_layer(state->focused_layer_surface) >= ZWLR_LAYER_SHELL_V1_LAYER_TOP && e_layer_surface_get_interactivity(state->focused_layer_surface) == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
}

static bool desktop_set_focus_raw_surface(struct e_server* server, struct wlr_surface* surface, bool replace_exclusive)
{
    assert(server);

    if (desktop_has_exclusive_focus(server) && !replace_exclusive)
    {
        e_log_info("desktop has exclusive focus!: not focusing on given surface");
        return false;
    }

    return e_seat_set_focus_surface(server->seat, surface);
}

static void desktop_set_active_view_container(struct e_server* server, struct e_view_container* view_container)
{
    assert(server);

    //don't set it to the same view container
    if (server->desktop_state.active_view_container == view_container)
        return;

    if (server->desktop_state.active_view_container != NULL)
        e_view_set_activated(server->desktop_state.active_view_container->view, false);

    server->desktop_state.active_view_container = view_container;

    if (view_container != NULL)
    {
        e_view_set_activated(view_container->view, true);
        e_container_raise_to_top(&view_container->base);

        struct e_output* output = (view_container->base.workspace != NULL) ? view_container->base.workspace->output : NULL;

        if (output != NULL && output->active_workspace != view_container->base.workspace)
            e_output_display_workspace(output, view_container->base.workspace);
    }
}

void e_desktop_set_focus_view_container(struct e_server* server, struct e_view_container* view_container)
{
    assert(server);

    desktop_set_active_view_container(server, view_container);

    if (view_container != NULL)
        desktop_set_focus_raw_surface(server, view_container->view->surface, false);
}

void e_desktop_set_focus_layer_surface(struct e_server* server, struct e_layer_surface* layer_surface)
{
    assert(server);

    //don't set it to the same layer surface
    if (server->desktop_state.focused_layer_surface == layer_surface)
        return;

    if (layer_surface == NULL)
    {
        server->desktop_state.focused_layer_surface = NULL;
        e_desktop_set_focus_view_container(server, server->desktop_state.active_view_container);
        return;
    }

    enum zwlr_layer_shell_v1_layer layer = e_layer_surface_get_layer(layer_surface);
    enum zwlr_layer_surface_v1_keyboard_interactivity interactivity = e_layer_surface_get_interactivity(layer_surface);

    if (interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
        return;

    bool replace_exclusive = (layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP && interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);

    //new exclusive must be on a higher layer
    if (desktop_has_exclusive_focus(server) && replace_exclusive)
        replace_exclusive = layer > e_layer_surface_get_layer(server->desktop_state.focused_layer_surface);

    struct wlr_surface* surface = layer_surface->scene_layer_surface_v1->layer_surface->surface;

    if (desktop_set_focus_raw_surface(server, surface, replace_exclusive))
        server->desktop_state.focused_layer_surface = layer_surface;
}

// Returns NULL on fail.
static struct e_layer_surface* layer_surface_try_from_surface(struct wlr_surface* surface)
{
    assert(surface);

    struct wlr_layer_surface_v1* wlr_layer_surface = wlr_layer_surface_v1_try_from_wlr_surface(surface);

    return (wlr_layer_surface != NULL) ? wlr_layer_surface->data : NULL;
}

void e_desktop_set_focus_surface(struct e_server* server, struct wlr_surface* surface)
{
    assert(server);

    if (surface == NULL)
        return;

    surface = wlr_surface_get_root_surface(surface);
    
    struct e_view_container* view_container = e_view_container_try_from_surface(server, surface);

    if (view_container != NULL)
    {
        e_desktop_set_focus_view_container(server, view_container);
        return;
    }

    struct e_layer_surface* layer_surface = layer_surface_try_from_surface(surface);

    if (layer_surface != NULL)
    {
        e_desktop_set_focus_layer_surface(server, layer_surface);
        return;
    }
}

// Returns view container currently in focus.
// Returns NULL if no view container has focus.
struct e_view_container* e_desktop_focused_view_container(struct e_server* server)
{
    assert(server);

    if (server->seat == NULL || server->seat->focus_surface == NULL)
        return NULL;

    return e_view_container_try_from_surface(server, server->seat->focus_surface);
}
