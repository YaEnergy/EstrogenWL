#include "desktop/windows/toplevel_window.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

#include "desktop/desktop.h"
#include "desktop/xdg_popup.h"
#include "desktop/tree/node.h"
#include "desktop/windows/window.h"

#include "input/cursor.h"

#include "util/log.h"

static bool e_toplevel_window_wants_floating(struct e_toplevel_window* toplevel_window)
{
    struct wlr_xdg_toplevel* xdg_toplevel = toplevel_window->xdg_toplevel;
    return (xdg_toplevel->current.min_width > 0 || xdg_toplevel->current.min_height > 0) && (xdg_toplevel->current.min_width == xdg_toplevel->current.max_width || xdg_toplevel->current.min_height == xdg_toplevel->current.max_height);
}

//surface is ready to be displayed
static void e_toplevel_window_map(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, map);

    e_window_map(&toplevel_window->base);

    //TODO: handle toplevel_window->xdg_toplevel->requested if surface is mapped
}

//surface no longer wants to be displayed
static void e_toplevel_window_unmap(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, unmap);

    e_window_unmap(&toplevel_window->base);
}

//new surface state got committed
static void e_toplevel_window_commit(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, commit);
        
    if (toplevel_window->xdg_toplevel->base->initial_commit)
    {
        if (e_toplevel_window_wants_floating(toplevel_window))
        {
            //0x0 size to let windows configure their size themselves, instead of forcing min or max size
            e_window_set_tiled(&toplevel_window->base, false);
            e_window_set_size(&toplevel_window->base, 0, 0);
            e_log_info("toplevel window wants floating");
        }
        else
        {
            e_window_set_tiled(&toplevel_window->base, true);
        }
    }

    toplevel_window->base.current.width = toplevel_window->xdg_toplevel->current.width;
    toplevel_window->base.current.height = toplevel_window->xdg_toplevel->current.height;

    //TODO: handle toplevel_window->xdg_toplevel->requested if surface is mapped
}

//new wlr_xdg_popup by toplevel window
static void e_toplevel_window_new_popup(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, new_popup);
    struct wlr_xdg_popup* xdg_popup = data;

    e_log_info("new popup by toplevel window: %s", toplevel_window->xdg_toplevel->title);

    e_xdg_popup_create(xdg_popup, toplevel_window->xdg_toplevel->base);
}

static void e_toplevel_window_request_fullscreen(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, request_fullscreen);

    //TODO: implement fullscreen

    //send empty configure to conform to xdg shell protocol if xdg surface is init
    if (toplevel_window->xdg_toplevel->base->initialized)
        wlr_xdg_surface_schedule_configure(toplevel_window->xdg_toplevel->base);
}

static void e_toplevel_window_request_maximize(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, request_maximize);

    if (!toplevel_window->xdg_toplevel->base->initialized)
        return;

    //must always send empty configure to conform to xdg shell protocol if xdg surface is init, even if we don't do anything
    if (toplevel_window->base.tiled)
        wlr_xdg_surface_schedule_configure(toplevel_window->xdg_toplevel->base);
    else
        e_window_maximize(&toplevel_window->base);
}

static void e_toplevel_window_request_move(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, request_move);
    //struct wlr_xdg_toplevel_move_event* event = data;

    //TODO: any client can request this, verify button serials
    
    struct e_desktop* desktop = toplevel_window->base.desktop;
    e_cursor_start_window_move(desktop->seat->cursor, &toplevel_window->base);
}

static void e_toplevel_window_request_resize(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, request_resize);
    struct wlr_xdg_toplevel_resize_event* event = data;

    //TODO: any client can request this, verify buttons serial
    
    struct e_desktop* desktop = toplevel_window->base.desktop;
    e_cursor_start_window_resize(desktop->seat->cursor, &toplevel_window->base, event->edges);
}

static void e_toplevel_window_set_title(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, set_title);

    toplevel_window->base.title = toplevel_window->xdg_toplevel->title;
}

//xdg_toplevel got destroyed
static void e_toplevel_window_destroy(struct wl_listener* listener, void* data)
{
    struct e_toplevel_window* toplevel_window = wl_container_of(listener, toplevel_window, destroy);

    wl_list_remove(&toplevel_window->map.link);
    wl_list_remove(&toplevel_window->unmap.link);
    wl_list_remove(&toplevel_window->commit.link);

    wl_list_remove(&toplevel_window->new_popup.link);

    wl_list_remove(&toplevel_window->request_fullscreen.link);
    wl_list_remove(&toplevel_window->request_maximize.link);
    wl_list_remove(&toplevel_window->request_move.link);
    wl_list_remove(&toplevel_window->request_resize.link);
    
    wl_list_remove(&toplevel_window->set_title.link);

    wl_list_remove(&toplevel_window->destroy.link);

    e_window_fini(&toplevel_window->base);

    free(toplevel_window);
}

static void e_toplevel_window_init_xdg_scene_tree(struct e_toplevel_window* toplevel_window)
{
    //create scene xdg surface for xdg toplevel and window, and set up window scene tree
    struct e_desktop* desktop = toplevel_window->base.desktop;
    toplevel_window->base.tree = wlr_scene_xdg_surface_create(desktop->pending, toplevel_window->xdg_toplevel->base);
    e_node_desc_create(&toplevel_window->base.tree->node, E_NODE_DESC_WINDOW, &toplevel_window->base);

    e_window_create_container_tree(&toplevel_window->base, desktop->pending);

    //allows popup scene trees to add themselves to this window's scene tree
    toplevel_window->xdg_toplevel->base->data = toplevel_window->base.tree;
}

static void e_window_toplevel_changed_tiling(struct e_window* window, bool tiled)
{
    assert(window && window->data);

    struct e_toplevel_window* toplevel_window = window->data;

    wlr_xdg_toplevel_set_tiled(toplevel_window->xdg_toplevel, tiled ? WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP : WLR_EDGE_NONE);
}

static uint32_t e_window_toplevel_configure(struct e_window* window, int lx, int ly, int width, int height)
{
    assert(window && window->tree && window->data);

    struct e_toplevel_window* toplevel_window = window->data;

    wlr_scene_node_set_position(&window->tree->node, lx, ly);

    //schedule empty configure if size remains the same
    if (window->current.width == width && window->current.height == height)
        return wlr_xdg_surface_schedule_configure(toplevel_window->xdg_toplevel->base);
    else
        return wlr_xdg_toplevel_set_size(toplevel_window->xdg_toplevel, width, height);
}

static void e_window_toplevel_send_close(struct e_window* window)
{
    assert(window && window->data);

    struct e_toplevel_window* toplevel_window = window->data;
    
    wlr_xdg_toplevel_send_close(toplevel_window->xdg_toplevel);
}

struct e_toplevel_window* e_toplevel_window_create(struct e_desktop* desktop, struct wlr_xdg_toplevel* xdg_toplevel)
{
    assert(desktop && xdg_toplevel);

    struct e_toplevel_window* toplevel_window = calloc(1, sizeof(*toplevel_window));

    if (toplevel_window == NULL)
    {
        e_log_error("failed to allocate toplevel_window");
        return NULL;
    }

    //give pointer to xdg toplevel
    toplevel_window->xdg_toplevel = xdg_toplevel;

    e_window_init(&toplevel_window->base, desktop, E_WINDOW_TOPLEVEL);
    toplevel_window->base.data = toplevel_window;

    toplevel_window->base.title = xdg_toplevel->title;
    toplevel_window->base.surface = xdg_toplevel->base->surface;

    toplevel_window->base.implementation.changed_tiled = e_window_toplevel_changed_tiling;
    toplevel_window->base.implementation.configure = e_window_toplevel_configure;
    toplevel_window->base.implementation.send_close = e_window_toplevel_send_close;

    e_toplevel_window_init_xdg_scene_tree(toplevel_window);

    // events

    // surface events

    toplevel_window->map.notify = e_toplevel_window_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel_window->map);

    toplevel_window->unmap.notify = e_toplevel_window_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel_window->unmap);

    toplevel_window->commit.notify = e_toplevel_window_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel_window->commit);

    // xdg surface events

    toplevel_window->new_popup.notify = e_toplevel_window_new_popup;
    wl_signal_add(&xdg_toplevel->base->events.new_popup, &toplevel_window->new_popup);

    // xdg toplevel events

    toplevel_window->request_fullscreen.notify = e_toplevel_window_request_fullscreen;
    wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel_window->request_fullscreen);

    toplevel_window->request_maximize.notify = e_toplevel_window_request_maximize;
    wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel_window->request_maximize);

    toplevel_window->request_move.notify = e_toplevel_window_request_move;
    wl_signal_add(&xdg_toplevel->events.request_move, &toplevel_window->request_move);

    toplevel_window->request_resize.notify = e_toplevel_window_request_resize;
    wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel_window->request_resize);

    toplevel_window->set_title.notify = e_toplevel_window_set_title;
    wl_signal_add(&xdg_toplevel->events.set_title, &toplevel_window->set_title);

    //window destroy event
    toplevel_window->destroy.notify = e_toplevel_window_destroy;
    wl_signal_add(&xdg_toplevel->events.destroy, &toplevel_window->destroy);
    
    //TODO: request resize, fullscreen, ... events

    return toplevel_window;
}