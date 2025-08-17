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
#include "desktop/output.h"
#include "desktop/tree/node.h"
#include "desktop/views/view.h"

#include "input/cursor.h"

#include "util/log.h"
#include "util/wl_macros.h"

/* Toplevel view popups */

// Returns NULL on fail.
static struct e_xdg_popup* xdg_popup_create(struct wlr_xdg_popup* xdg_popup, struct e_view* view, struct wlr_scene_tree* parent);

static void xdg_popup_handle_new_popup(struct wl_listener* listener, void* data)
{
    struct e_xdg_popup* popup = wl_container_of(listener, popup, new_popup);
    struct wlr_xdg_popup* new_xdg_popup = data;

    e_log_info("popup creates new popup!");
    xdg_popup_create(new_xdg_popup, popup->view, popup->tree);
}

static void xdg_popup_unconstrain(struct e_xdg_popup* popup)
{
    assert(popup);

    if (popup == NULL)
        return;

    struct wlr_box toplevel_popup_space = popup->view->popup_space;
    toplevel_popup_space.x += popup->view->root_geometry.x;
    toplevel_popup_space.y += popup->view->root_geometry.y;

    wlr_xdg_popup_unconstrain_from_box(popup->xdg_popup, &toplevel_popup_space);
}

static void xdg_popup_handle_reposition(struct wl_listener* listener, void* data)
{
    struct e_xdg_popup* popup = wl_container_of(listener, popup, reposition);

    xdg_popup_unconstrain(popup);
}

static void xdg_popup_handle_commit(struct wl_listener* listener, void* data)
{
    struct e_xdg_popup* popup = wl_container_of(listener, popup, commit);
    
    if (popup->xdg_popup->base->initial_commit)
    {
        xdg_popup_unconstrain(popup);
        wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
    }
}

static void xdg_popup_handle_destroy(struct wl_listener* listener, void* data)
{
    struct e_xdg_popup* popup = wl_container_of(listener, popup, destroy);

    SIGNAL_DISCONNECT(popup->reposition);
    SIGNAL_DISCONNECT(popup->new_popup);
    SIGNAL_DISCONNECT(popup->commit);
    SIGNAL_DISCONNECT(popup->destroy);

    free(popup);
}

// Returns NULL on fail.
static struct e_xdg_popup* xdg_popup_create(struct wlr_xdg_popup* xdg_popup, struct e_view* view, struct wlr_scene_tree* parent)
{
    assert(xdg_popup && parent);

    struct e_xdg_popup* popup = calloc(1, sizeof(*popup));

    if (popup == NULL)
        return NULL;

    popup->xdg_popup = xdg_popup;
    popup->view = view;

    //create popup's scene tree, and add popup to scene tree of parent
    popup->tree = wlr_scene_xdg_surface_create(parent, xdg_popup->base);
    e_node_desc_create(&popup->tree->node, E_NODE_DESC_XDG_POPUP, popup);

    //events

    SIGNAL_CONNECT(xdg_popup->events.reposition, popup->reposition, xdg_popup_handle_reposition);
    SIGNAL_CONNECT(xdg_popup->base->events.new_popup, popup->new_popup, xdg_popup_handle_new_popup);
    SIGNAL_CONNECT(xdg_popup->base->surface->events.commit, popup->commit, xdg_popup_handle_commit);
    SIGNAL_CONNECT(xdg_popup->events.destroy, popup->destroy, xdg_popup_handle_destroy);

    return popup;
}

/* Toplevel view */

//TODO: use toplevel_view_impl_* instead of e_view_toplevel for better consistency across other files

// Returns size hints of view.
static struct e_view_size_hints e_view_toplevel_get_size_hints(struct e_view* view)
{
    assert(view);

    struct e_toplevel_view* toplevel = view->data;

    return (struct e_view_size_hints){
        .min_width = toplevel->xdg_toplevel->current.min_width,
        .min_height = toplevel->xdg_toplevel->current.min_height,

        .max_width = toplevel->xdg_toplevel->current.max_width,
        .max_height = toplevel->xdg_toplevel->current.max_height,
    };
}

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

static void toplevel_view_update_geometry(struct e_toplevel_view* toplevel_view)
{
    assert(toplevel_view);

    toplevel_view->base.root_geometry = toplevel_view->xdg_toplevel->base->geometry;
    toplevel_view->base.width = toplevel_view->xdg_toplevel->current.width;
    toplevel_view->base.height = toplevel_view->xdg_toplevel->current.height;
}

//surface is ready to be displayed
static void e_toplevel_view_map(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, map);

    wlr_xdg_toplevel_set_wm_capabilities(toplevel_view->xdg_toplevel, WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
    toplevel_view_update_geometry(toplevel_view);
    
    if (toplevel_view->xdg_toplevel->requested.fullscreen)
    {
        struct e_output* output = NULL;

        if (toplevel_view->xdg_toplevel->requested.fullscreen_output != NULL)
            output = toplevel_view->xdg_toplevel->requested.fullscreen_output->data;

        e_view_map(&toplevel_view->base, true, output);
    }
    else
    {
        e_view_map(&toplevel_view->base, false, NULL);
    }

    //TODO: handle requested maximized, minimized and other stuff
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

    toplevel_view_update_geometry(toplevel_view);

    wl_signal_emit_mutable(&toplevel_view->base.events.commit, NULL);
}

//new wlr_xdg_popup by toplevel view
static void e_toplevel_view_new_popup(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, new_popup);
    struct wlr_xdg_popup* xdg_popup = data;

    e_log_info("new popup by toplevel view: %s", toplevel_view->xdg_toplevel->title);

    xdg_popup_create(xdg_popup, &toplevel_view->base, toplevel_view->base.tree);
}

static void e_toplevel_view_request_fullscreen(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, request_fullscreen);

    struct wlr_xdg_toplevel* toplevel = toplevel_view->xdg_toplevel;

    struct e_view_request_fullscreen_event view_event = {
        .view = &toplevel_view->base,
        .fullscreen = toplevel->requested.fullscreen,
        .output = NULL
    };

    if (toplevel->requested.fullscreen_output != NULL)
        view_event.output = toplevel->requested.fullscreen_output->data;

    wl_signal_emit_mutable(&toplevel_view->base.events.request_fullscreen, &view_event);
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

    struct e_view_request_move_event view_event = {
        .view = &toplevel_view->base
    };

    wl_signal_emit_mutable(&toplevel_view->base.events.request_move, &view_event);
}

static void e_toplevel_view_request_resize(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, request_resize);
    struct wlr_xdg_toplevel_resize_event* event = data;
    
    struct e_view_request_resize_event view_event = {
        .view = &toplevel_view->base,
        .edges = event->edges,
    };

    wl_signal_emit_mutable(&toplevel_view->base.events.request_resize, &view_event);
}

static void e_toplevel_view_set_title(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, set_title);

    toplevel_view->base.title = toplevel_view->xdg_toplevel->title;
}

static void e_toplevel_view_set_app_id(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, set_app_id);

    toplevel_view->base.app_id = toplevel_view->xdg_toplevel->app_id;
}

//xdg_toplevel got destroyed
static void e_toplevel_view_destroy(struct wl_listener* listener, void* data)
{
    struct e_toplevel_view* toplevel_view = wl_container_of(listener, toplevel_view, destroy);

    wl_signal_emit_mutable(&toplevel_view->base.events.destroy, NULL);

    SIGNAL_DISCONNECT(toplevel_view->map);
    SIGNAL_DISCONNECT(toplevel_view->unmap);
    SIGNAL_DISCONNECT(toplevel_view->commit);

    SIGNAL_DISCONNECT(toplevel_view->new_popup);

    SIGNAL_DISCONNECT(toplevel_view->request_fullscreen);
    SIGNAL_DISCONNECT(toplevel_view->request_maximize);
    SIGNAL_DISCONNECT(toplevel_view->request_move);
    SIGNAL_DISCONNECT(toplevel_view->request_resize);
    SIGNAL_DISCONNECT(toplevel_view->set_title);
    SIGNAL_DISCONNECT(toplevel_view->set_app_id);

    SIGNAL_DISCONNECT(toplevel_view->destroy);

    e_view_fini(&toplevel_view->base);

    free(toplevel_view);
}

// Sets the tiled state of the view.
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

    e_view_base_set_activated(&toplevel_view->base, activated);
    wlr_xdg_toplevel_set_activated(toplevel_view->xdg_toplevel, activated);
}

// Set the fullscreen mode of the view.
static void e_view_toplevel_set_fullscreen(struct e_view* view, bool fullscreen)
{
    assert(view);

    struct e_toplevel_view* toplevel_view = view->data;

    e_view_base_set_fullscreen(&toplevel_view->base, fullscreen);
    wlr_xdg_toplevel_set_fullscreen(toplevel_view->xdg_toplevel, fullscreen);
}

static void e_view_toplevel_configure(struct e_view* view, int lx, int ly, int width, int height)
{
    assert(view && view->content_tree && view->data);

    struct e_toplevel_view* toplevel_view = view->data;

    #if E_VERBOSE
    e_log_info("toplevel configure");
    #endif
    
    wlr_xdg_toplevel_set_size(toplevel_view->xdg_toplevel, width, height);
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

static const struct e_view_impl view_toplevel_implementation = {
    .get_size_hints = e_view_toplevel_get_size_hints,

    .set_tiled = e_view_toplevel_set_tiled,
    
    .set_activated = e_view_toplevel_set_activated,
    .set_fullscreen = e_view_toplevel_set_fullscreen,

    .configure = e_view_toplevel_configure,
    .create_content_tree = e_view_toplevel_create_content_tree,
    .wants_floating = e_view_toplevel_wants_floating,
    .send_close = e_view_toplevel_send_close,
};

struct e_toplevel_view* e_toplevel_view_create(struct e_server* server, struct wlr_xdg_toplevel* xdg_toplevel)
{
    assert(server && xdg_toplevel);

    struct e_toplevel_view* toplevel_view = calloc(1, sizeof(*toplevel_view));

    if (toplevel_view == NULL)
    {
        e_log_error("failed to allocate toplevel_view");
        return NULL;
    }

    //give pointer to xdg toplevel
    toplevel_view->xdg_toplevel = xdg_toplevel;

    e_view_init(&toplevel_view->base, E_VIEW_TOPLEVEL, toplevel_view, &view_toplevel_implementation, server);

    toplevel_view->base.title = xdg_toplevel->title;
    toplevel_view->base.surface = xdg_toplevel->base->surface;

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
    SIGNAL_CONNECT(xdg_toplevel->events.set_app_id, toplevel_view->set_app_id, e_toplevel_view_set_app_id);

    SIGNAL_CONNECT(xdg_toplevel->events.destroy, toplevel_view->destroy, e_toplevel_view_destroy);

    return toplevel_view;
}
