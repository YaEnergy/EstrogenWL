#include "desktop/views/toplevel_view.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>

#include "desktop/desktop.h"
#include "desktop/xdg_popup.h"
#include "desktop/tree/node.h"
#include "desktop/views/view.h"

#include "input/cursor.h"

#include "util/log.h"

//surface is ready to be displayed
static void e_toplevel_view_map(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, map);

    e_view_map(&toplevel_view->base);

    //TODO: handle toplevel_view->xdg_toplevel->requested if surface is mapped
}

//surface no longer wants to be displayed
static void e_toplevel_view_unmap(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, unmap);

    e_view_unmap(&toplevel_view->base);
}

// New surface state got committed.
static void e_toplevel_view_commit(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, commit);
        
    if (toplevel_view->xdg_toplevel->base->initial_commit)
    {
        #if E_VERBOSE
        e_log_info("toplevel view initial commit");
        #endif

        //0x0 size to let views configure their size themselves, instead of forcing min or max size
        wlr_xdg_toplevel_set_size(toplevel_view->xdg_toplevel, 0, 0);
        //TODO: add wm capabilities
        wlr_xdg_toplevel_set_wm_capabilities(toplevel_view->xdg_toplevel, 0);
    }

    if (!toplevel_view->xdg_toplevel->base->surface->mapped)
        return;

    toplevel_view->base.current.width = toplevel_view->xdg_toplevel->current.width;
    toplevel_view->base.current.height = toplevel_view->xdg_toplevel->current.height;

    //TODO: handle toplevel_view->xdg_toplevel->requested if surface is mapped
}

//new wlr_xdg_popup by toplevel view
static void e_toplevel_view_new_popup(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, new_popup);
    struct wlr_xdg_popup* xdg_popup = data;

    e_log_info("new popup by toplevel view: %s", toplevel_view->xdg_toplevel->title);

    e_xdg_popup_create(xdg_popup, toplevel_view->xdg_toplevel->base);
}

static void e_toplevel_view_request_fullscreen(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, request_fullscreen);

    //TODO: implement fullscreen

    //send empty configure to conform to xdg shell protocol if xdg surface is init
    if (toplevel_view->xdg_toplevel->base->initialized)
        wlr_xdg_surface_schedule_configure(toplevel_view->xdg_toplevel->base);
}

static void e_toplevel_view_request_maximize(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, request_maximize);

    if (!toplevel_view->xdg_toplevel->base->initialized)
        return;

    //must always send empty configure to conform to xdg shell protocol if xdg surface is init, even if we don't do anything
    if (toplevel_view->base.tiled)
        wlr_xdg_surface_schedule_configure(toplevel_view->xdg_toplevel->base);
    else
        e_view_maximize(&toplevel_view->base);
}

static void e_toplevel_view_request_move(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, request_move);
    struct wlr_xdg_toplevel_move_event* event = data;
    
    if (!wlr_seat_client_validate_event_serial(event->seat, event->serial))
    {
        e_log_error("e_toplevel_view_request_move: invalid event serial!");
        return;
    }
    
    struct e_desktop* desktop = toplevel_view->base.desktop;
    e_cursor_start_window_move(desktop->seat->cursor, toplevel_view->base.container);
}

static void e_toplevel_view_request_resize(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, request_resize);
    struct wlr_xdg_toplevel_resize_event* event = data;

    if (!wlr_seat_client_validate_event_serial(event->seat, event->serial))
    {
        e_log_error("e_toplevel_view_request_resize: invalid event serial!");
        return;
    }
    
    struct e_desktop* desktop = toplevel_view->base.desktop;
    e_cursor_start_window_resize(desktop->seat->cursor, toplevel_view->base.container, event->edges);
}

static void e_toplevel_view_set_title(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, set_title);

    toplevel_view->base.title = toplevel_view->xdg_toplevel->title;
    wl_signal_emit(&toplevel_view->base.events.set_title, toplevel_view->xdg_toplevel->title);
}

//xdg_toplevel got destroyed
static void e_toplevel_view_destroy(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, destroy);

    wl_list_remove(&toplevel_view->map.link);
    wl_list_remove(&toplevel_view->unmap.link);
    wl_list_remove(&toplevel_view->commit.link);

    wl_list_remove(&toplevel_view->new_popup.link);

    wl_list_remove(&toplevel_view->request_fullscreen.link);
    wl_list_remove(&toplevel_view->request_maximize.link);
    wl_list_remove(&toplevel_view->request_move.link);
    wl_list_remove(&toplevel_view->request_resize.link);
    
    wl_list_remove(&toplevel_view->set_title.link);

    wl_list_remove(&toplevel_view->destroy.link);

    e_view_fini(&toplevel_view->base);

    free(toplevel_view);
}

static void e_toplevel_view_init_xdg_scene_tree(struct e_toplevel_view* toplevel_view)
{
    //create scene xdg surface for xdg toplevel and view, and set up view scene tree
    toplevel_view->base.tree = wlr_scene_xdg_surface_create(toplevel_view->base.desktop->pending, toplevel_view->xdg_toplevel->base);
    e_node_desc_create(&toplevel_view->base.tree->node, E_NODE_DESC_VIEW, &toplevel_view->base);

    //allows popup scene trees to add themselves to this view's scene tree
    toplevel_view->xdg_toplevel->base->data = toplevel_view->base.tree;
}

static void e_view_toplevel_set_tiled(struct e_view* view, bool tiled)
{
    assert(view && view->data);

    struct e_toplevel_view* toplevel_view = view->data;

    wlr_xdg_toplevel_set_tiled(toplevel_view->xdg_toplevel, tiled ? WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP : WLR_EDGE_NONE);
}

static uint32_t e_view_toplevel_configure(struct e_view* view, int lx, int ly, int width, int height)
{
    assert(view && view->tree && view->data);

    struct e_toplevel_view* toplevel_view = view->data;

    wlr_scene_node_set_position(&view->tree->node, lx, ly);

    #if E_VERBOSE
    e_log_info("toplevel configure");
    #endif

    //schedule empty configure if size remains the same
    if (view->current.width == width && view->current.height == height)
        return wlr_xdg_surface_schedule_configure(toplevel_view->xdg_toplevel->base);
    else
        return wlr_xdg_toplevel_set_size(toplevel_view->xdg_toplevel, width, height);
}

static bool e_view_toplevel_wants_floating(struct e_view* view)
{
    assert(view);

    struct e_toplevel_view* toplevel_view = view->data;
    struct wlr_xdg_toplevel* xdg_toplevel = toplevel_view->xdg_toplevel;
    
    //does surface force a specific size?
    return (xdg_toplevel->current.min_width > 0 || xdg_toplevel->current.min_height > 0) && (xdg_toplevel->current.min_width == xdg_toplevel->current.max_width || xdg_toplevel->current.min_height == xdg_toplevel->current.max_height);
}

static void e_view_toplevel_send_close(struct e_view* view)
{
    assert(view && view->data);

    struct e_toplevel_view* toplevel_view = view->data;
    
    wlr_xdg_toplevel_send_close(toplevel_view->xdg_toplevel);
}

struct e_toplevel_view* e_toplevel_view_create(struct e_desktop* desktop, struct wlr_xdg_toplevel* xdg_toplevel)
{
    assert(desktop && xdg_toplevel);

    struct e_toplevel_view* toplevel_view = calloc(1, sizeof(*toplevel_view));

    if (toplevel_view == NULL)
    {
        e_log_error("failed to allocate toplevel_view");
        return NULL;
    }

    //give pointer to xdg toplevel
    toplevel_view->xdg_toplevel = xdg_toplevel;

    e_view_init(&toplevel_view->base, desktop, E_VIEW_TOPLEVEL, toplevel_view);

    toplevel_view->base.title = xdg_toplevel->title;
    toplevel_view->base.surface = xdg_toplevel->base->surface;

    toplevel_view->base.implementation.set_tiled = e_view_toplevel_set_tiled;
    toplevel_view->base.implementation.configure = e_view_toplevel_configure;
    toplevel_view->base.implementation.wants_floating = e_view_toplevel_wants_floating;
    toplevel_view->base.implementation.send_close = e_view_toplevel_send_close;

    e_toplevel_view_init_xdg_scene_tree(toplevel_view);

    // events

    // surface events

    toplevel_view->map.notify = e_toplevel_view_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel_view->map);

    toplevel_view->unmap.notify = e_toplevel_view_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel_view->unmap);

    toplevel_view->commit.notify = e_toplevel_view_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel_view->commit);

    // xdg surface events

    toplevel_view->new_popup.notify = e_toplevel_view_new_popup;
    wl_signal_add(&xdg_toplevel->base->events.new_popup, &toplevel_view->new_popup);

    // xdg toplevel events

    toplevel_view->request_fullscreen.notify = e_toplevel_view_request_fullscreen;
    wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel_view->request_fullscreen);

    toplevel_view->request_maximize.notify = e_toplevel_view_request_maximize;
    wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel_view->request_maximize);

    toplevel_view->request_move.notify = e_toplevel_view_request_move;
    wl_signal_add(&xdg_toplevel->events.request_move, &toplevel_view->request_move);

    toplevel_view->request_resize.notify = e_toplevel_view_request_resize;
    wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel_view->request_resize);

    toplevel_view->set_title.notify = e_toplevel_view_set_title;
    wl_signal_add(&xdg_toplevel->events.set_title, &toplevel_view->set_title);

    //view destroy event
    toplevel_view->destroy.notify = e_toplevel_view_destroy;
    wl_signal_add(&xdg_toplevel->events.destroy, &toplevel_view->destroy);
    
    //TODO: request resize, fullscreen, ... events

    return toplevel_view;
}