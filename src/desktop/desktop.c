#include "desktop/desktop.h"

#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include "input/seat.h"
#include "input/cursor.h"

#include "desktop/views/view.h"
#include "desktop/tree/container.h"
#include "desktop/tree/workspace.h"
#include "desktop/layer_shell.h"
#include "desktop/output.h"

#include "util/log.h"

#include "server.h"

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

// Set seat focus on a view container if possible, and does whatever is necessary to do so.
void e_desktop_focus_view_container(struct e_view_container* view_container)
{
    assert(view_container);

    #if E_VERBOSE
    e_log_info("desktop focus on view container");
    #endif

    if (!view_container->view->mapped || view_container->base.workspace == NULL || view_container->base.workspace->output == NULL)
    {
        e_log_error("e_desktop_focus_view_container: view is not visible, won't focus");
        return;
    }

    if (e_seat_focus_surface(view_container->base.server->seat, view_container->view->surface))
    {
        struct e_output* output = view_container->base.workspace->output;

        if (output->active_workspace != view_container->base.workspace)
            e_output_display_workspace(output, view_container->base.workspace);

        e_view_set_activated(view_container->view, true);
        e_container_raise_to_top(&view_container->base);
    }
}

// Set seat focus on a layer surface if possible.
void e_desktop_focus_layer_surface(struct e_layer_surface* layer_surface)
{
    assert(layer_surface);

    e_seat_focus_surface(layer_surface->server->seat, layer_surface->scene_layer_surface_v1->layer_surface->surface);
}

// Gets the type of surface (view container or layer surface) and sets seat focus.
// This will do nothing if surface isn't of a type that should be focused on by the desktop's seat.
void e_desktop_focus_surface(struct e_server* server, struct wlr_surface* surface)
{
    assert(server && surface);

    if (server == NULL)
    {
        e_log_error("e_desktop_focus_surface: server is NULL!");
        return;
    }

    if (server == NULL)
    {
        e_log_error("e_desktop_focus_surface: surface is NULL!");
        return;
    }

    struct e_seat* seat = server->seat;

    //focus on views containers

    struct wlr_surface* root_surface = wlr_surface_get_root_surface(surface);
    struct e_view_container* view_container = e_view_container_try_from_surface(server, root_surface);

    if (view_container != NULL && !e_seat_has_focus(seat, view_container->view->surface))
    {
        e_desktop_focus_view_container(view_container);
        return;
    }

    //focus on layer surfaces that request on demand interactivity

    struct wlr_layer_surface_v1* wlr_layer_surface = wlr_layer_surface_v1_try_from_wlr_surface(surface);

    //is layer surface that allows focus?
    if (wlr_layer_surface != NULL && wlr_layer_surface->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
    {
        struct e_layer_surface* layer_surface = wlr_layer_surface->data;

        e_desktop_focus_layer_surface(layer_surface);
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

void e_desktop_clear_focus(struct e_server* server)
{
    assert(server);

    if (server == NULL)
    {
        e_log_error("e_desktop_clear_focus: server is NULL!");
        return;
    }

    struct e_view_container* focused_view_container = e_desktop_focused_view_container(server);

    if (focused_view_container != NULL)
        e_view_set_activated(focused_view_container->view, false);

    e_seat_clear_focus(server->seat);
}