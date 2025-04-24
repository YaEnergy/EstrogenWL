#include "desktop/xdg_shell.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>
#include <wlr/util/box.h>

#include "desktop/desktop.h"
#include "desktop/xdg_popup.h"
#include "desktop/tree/node.h"
#include "desktop/views/view.h"

#include "input/cursor.h"

#include "util/log.h"
#include "util/wl_macros.h"

// Create a scene tree displaying this view's surfaces and subsurfaces.
static struct wlr_scene_tree* e_view_toplevel_create_content_tree(struct e_view* view)
{
    assert(view);

    struct e_toplevel_view* toplevel_view = view->data;

    //create scene xdg surface for xdg toplevel and view, and set up view scene tree
    struct wlr_scene_tree* tree = wlr_scene_xdg_surface_create(view->tree, toplevel_view->xdg_toplevel->base);
    e_node_desc_create(&tree->node, E_NODE_DESC_VIEW, view);

    //allows popup scene trees to add themselves to this view's scene tree
    toplevel_view->xdg_toplevel->base->data = tree;

    return tree;
}

//surface is ready to be displayed
static void e_toplevel_view_map(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, map);

    //TODO: add actual wm capabilities
    wlr_xdg_toplevel_set_wm_capabilities(toplevel_view->xdg_toplevel, 0);
    
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
        return;
    }

    if (!toplevel_view->xdg_toplevel->base->surface->mapped)
        return;

    toplevel_view->base.current.x = toplevel_view->scheduled_x;
    toplevel_view->base.current.y = toplevel_view->scheduled_y;
    
    toplevel_view->base.current.width = toplevel_view->xdg_toplevel->current.width;
    toplevel_view->base.current.height = toplevel_view->xdg_toplevel->current.height;

    e_view_moved(&toplevel_view->base);

    //Now that we've finished the changes that were scheduled, we can schedule the next changes.
    if (e_view_has_pending_changes(&toplevel_view->base))
        e_view_configure_pending(&toplevel_view->base);
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
    //TODO: maximizing
    wlr_xdg_surface_schedule_configure(toplevel_view->xdg_toplevel->base);
}

static void e_toplevel_view_request_move(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, request_move);

    struct e_desktop* desktop = toplevel_view->base.desktop;
    e_cursor_start_view_move(desktop->seat->cursor, &toplevel_view->base);
}

static void e_toplevel_view_request_resize(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, request_resize);
    struct wlr_xdg_toplevel_resize_event* event = data;
    
    struct e_desktop* desktop = toplevel_view->base.desktop;
    e_cursor_start_view_resize(desktop->seat->cursor, &toplevel_view->base, event->edges);
}

static void e_toplevel_view_set_title(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, set_title);

    toplevel_view->base.title = toplevel_view->xdg_toplevel->title;
}

//xdg_toplevel got destroyed
static void e_toplevel_view_destroy(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, destroy);

    SIGNAL_DISCONNECT(toplevel_view->map);
    SIGNAL_DISCONNECT(toplevel_view->unmap);
    SIGNAL_DISCONNECT(toplevel_view->commit);

    SIGNAL_DISCONNECT(toplevel_view->new_popup);

    SIGNAL_DISCONNECT(toplevel_view->request_fullscreen);
    SIGNAL_DISCONNECT(toplevel_view->request_maximize);
    SIGNAL_DISCONNECT(toplevel_view->request_move);
    SIGNAL_DISCONNECT(toplevel_view->request_resize);
    SIGNAL_DISCONNECT(toplevel_view->set_title);

    SIGNAL_DISCONNECT(toplevel_view->destroy);

    e_view_fini(&toplevel_view->base);

    free(toplevel_view);
}

// Set tiled state of the view.
static void e_view_toplevel_set_tiled(struct e_view* view, bool tiled)
{
    assert(view && view->data);

    struct e_toplevel_view* toplevel_view = view->data;

    wlr_xdg_toplevel_set_tiled(toplevel_view->xdg_toplevel, tiled ? WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP : WLR_EDGE_NONE);
}

// Set activated state of the view.
static void e_view_toplevel_set_activated(struct e_view* view, bool activated)
{
    assert(view);

    struct e_toplevel_view* toplevel_view = view->data;

    wlr_xdg_toplevel_set_activated(toplevel_view->xdg_toplevel, activated);

    if (activated)
        wlr_scene_node_raise_to_top(&view->tree->node);
}

static bool toplevel_size_configure_is_scheduled(struct wlr_xdg_toplevel* xdg_toplevel)
{
    assert(xdg_toplevel);

    return xdg_toplevel->scheduled.width != xdg_toplevel->current.width || xdg_toplevel->scheduled.height != xdg_toplevel->current.height;
}

static void e_view_toplevel_configure(struct e_view* view, int lx, int ly, int width, int height)
{
    assert(view && view->content_tree && view->data);

    struct e_toplevel_view* toplevel_view = view->data;

    #if E_VERBOSE
    e_log_info("toplevel configure");
    #endif
    
    view->pending = (struct wlr_box){lx, ly, width, height};

    //configure is already scheduled, commit will start next one
    if (toplevel_size_configure_is_scheduled(toplevel_view->xdg_toplevel))
        return;

    //if size remains the same then just move the container node
    if (view->current.width == width && view->current.height == height)
    {
        toplevel_view->base.current.x = lx;
        toplevel_view->base.current.y = ly;
        e_view_moved(&toplevel_view->base);
    }
    else
    {
        //wait until surface commit to apply x & y
        toplevel_view->scheduled_x = lx;
        toplevel_view->scheduled_y = ly;
        wlr_xdg_toplevel_set_size(toplevel_view->xdg_toplevel, width, height);
    }
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
    toplevel_view->base.implementation.set_activated = e_view_toplevel_set_activated;
    toplevel_view->base.implementation.configure = e_view_toplevel_configure;
    toplevel_view->base.implementation.create_content_tree = e_view_toplevel_create_content_tree;
    toplevel_view->base.implementation.wants_floating = e_view_toplevel_wants_floating;
    toplevel_view->base.implementation.send_close = e_view_toplevel_send_close;

    // events

    // surface events

    SIGNAL_CONNECT(xdg_toplevel->base->surface->events.map, toplevel_view->map, e_toplevel_view_map);
    SIGNAL_CONNECT(xdg_toplevel->base->surface->events.unmap, toplevel_view->unmap, e_toplevel_view_unmap);
    SIGNAL_CONNECT(xdg_toplevel->base->surface->events.commit, toplevel_view->commit, e_toplevel_view_commit);

    // xdg surface events

    SIGNAL_CONNECT(xdg_toplevel->base->events.new_popup, toplevel_view->new_popup, e_toplevel_view_new_popup);

    // xdg toplevel events

    SIGNAL_CONNECT(xdg_toplevel->events.request_fullscreen, toplevel_view->request_fullscreen, e_toplevel_view_request_fullscreen);
    SIGNAL_CONNECT(xdg_toplevel->events.request_maximize, toplevel_view->request_maximize, e_toplevel_view_request_maximize);
    SIGNAL_CONNECT(xdg_toplevel->events.request_move, toplevel_view->request_move, e_toplevel_view_request_move);
    SIGNAL_CONNECT(xdg_toplevel->events.request_resize, toplevel_view->request_resize, e_toplevel_view_request_resize);
    SIGNAL_CONNECT(xdg_toplevel->events.set_title, toplevel_view->set_title, e_toplevel_view_set_title);

    SIGNAL_CONNECT(xdg_toplevel->events.destroy, toplevel_view->destroy, e_toplevel_view_destroy);

    return toplevel_view;
}
