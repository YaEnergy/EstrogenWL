#include "desktop/foreign_toplevel.h"

#include <assert.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>

#include "util/wl_macros.h"

#include "desktop/views/view.h"
#include "desktop/output.h"

#include "server.h"

/* ext handle */

static void ext_handle_handle_destroy(struct wl_listener* listener, void* data)
{
    struct e_ext_foreign_toplevel* ext_handle = wl_container_of(listener, ext_handle, destroy);

    SIGNAL_DISCONNECT(ext_handle->destroy);
}

static void view_get_ext_handle_state(const struct e_view* view, struct wlr_ext_foreign_toplevel_handle_v1_state* state)
{
    assert(view && state);

    state->title = view->title; //TODO: is this okay?
    state->app_id = NULL;
    //TODO: get app id
}

static void ext_handle_fini(struct e_ext_foreign_toplevel* ext_handle)
{
    assert(ext_handle);

    wlr_ext_foreign_toplevel_handle_v1_destroy(ext_handle->handle);
}

static void ext_handle_init(struct e_ext_foreign_toplevel* ext_handle, struct e_view* view)
{
    assert(ext_handle && view);

    ext_handle->view = view;

    struct wlr_ext_foreign_toplevel_handle_v1_state state = {0};
    view_get_ext_handle_state(view, &state);

    ext_handle->handle = wlr_ext_foreign_toplevel_handle_v1_create(view->server->foreign_toplevel_list, &state);    

    SIGNAL_CONNECT(ext_handle->handle->events.destroy, ext_handle->destroy, ext_handle_handle_destroy);
}

/* wlr handle */

static void wlr_handle_handle_request_activate(struct wl_listener* listener, void* data)
{
    struct e_wlr_foreign_toplevel* wlr_handle = wl_container_of(listener, wlr_handle, request_activate);
    //struct wlr_foreign_toplevel_handle_v1_activated_event* event = data;
    //TODO: handle this event for a specific seat, if we ever have support for multiple seats

    wl_signal_emit_mutable(&wlr_handle->view->events.request_activate, NULL);
}

static void wlr_handle_handle_request_fullscreen(struct wl_listener* listener, void* data)
{
    struct e_wlr_foreign_toplevel* wlr_handle = wl_container_of(listener, wlr_handle, request_fullscreen);
    struct wlr_foreign_toplevel_handle_v1_fullscreen_event* event = data;

    struct e_view_request_fullscreen_event view_event = {
        .view = wlr_handle->view,
        .fullscreen = event->fullscreen,
        .output = (event->fullscreen && event->output != NULL && event->output->data != NULL) ? event->output->data : NULL
    };

    wl_signal_emit_mutable(&wlr_handle->view->events.request_fullscreen, &view_event);
}

static void wlr_handle_handle_request_close(struct wl_listener* listener, void* data)
{
    struct e_wlr_foreign_toplevel* wlr_handle = wl_container_of(listener, wlr_handle, request_close);

    e_view_send_close(wlr_handle->view);
}

static void wlr_handle_handle_destroy(struct wl_listener* listener, void* data)
{
    struct e_wlr_foreign_toplevel* wlr_handle = wl_container_of(listener, wlr_handle, destroy);

    SIGNAL_DISCONNECT(wlr_handle->request_activate);
    SIGNAL_DISCONNECT(wlr_handle->request_fullscreen);
    SIGNAL_DISCONNECT(wlr_handle->request_close);

    SIGNAL_DISCONNECT(wlr_handle->destroy);   
}

static void wlr_handle_fini(struct e_wlr_foreign_toplevel* wlr_handle)
{
    assert(wlr_handle);

    wlr_foreign_toplevel_handle_v1_destroy(wlr_handle->handle);
}

static void wlr_handle_init(struct e_wlr_foreign_toplevel* wlr_handle, struct e_view* view)
{
    assert(wlr_handle && view);

    wlr_handle->view = view;
    wlr_handle->handle = wlr_foreign_toplevel_handle_v1_create(view->server->foreign_toplevel_manager);

    SIGNAL_CONNECT(wlr_handle->handle->events.request_activate, wlr_handle->request_activate, wlr_handle_handle_request_activate);
    SIGNAL_CONNECT(wlr_handle->handle->events.request_fullscreen, wlr_handle->request_fullscreen, wlr_handle_handle_request_fullscreen);
    SIGNAL_CONNECT(wlr_handle->handle->events.request_close, wlr_handle->request_close, wlr_handle_handle_request_close);

    SIGNAL_CONNECT(wlr_handle->handle->events.destroy, wlr_handle->destroy, wlr_handle_handle_destroy);

    //TODO: initial state
}

/* foreign toplevel */

// Returns NULL on fail.
struct e_foreign_toplevel* e_foreign_toplevel_create(struct e_view* view)
{
    assert(view);

    struct e_foreign_toplevel* foreign_toplevel = calloc(1, sizeof(*foreign_toplevel));

    if (foreign_toplevel == NULL)
        return NULL;

    ext_handle_init(&foreign_toplevel->ext, view);
    wlr_handle_init(&foreign_toplevel->wlr, view);

    if (view->output != NULL)
        e_foreign_toplevel_output_enter(foreign_toplevel, view->output);

    e_foreign_toplevel_set_activated(foreign_toplevel, view->activated);
    e_foreign_toplevel_set_fullscreen(foreign_toplevel, view->fullscreen);
    //TODO: initial state

    return foreign_toplevel;
}

void e_foreign_toplevel_output_enter(struct e_foreign_toplevel* foreign_toplevel, struct e_output* output)
{
    assert(foreign_toplevel && output);

    wlr_foreign_toplevel_handle_v1_output_enter(foreign_toplevel->wlr.handle, output->wlr_output);
}

void e_foreign_toplevel_output_leave(struct e_foreign_toplevel* foreign_toplevel, struct e_output* output)
{
    assert(foreign_toplevel && output);

    wlr_foreign_toplevel_handle_v1_output_leave(foreign_toplevel->wlr.handle, output->wlr_output);
}

void e_foreign_toplevel_set_activated(struct e_foreign_toplevel* foreign_toplevel, bool activated)
{
    assert(foreign_toplevel);

    wlr_foreign_toplevel_handle_v1_set_activated(foreign_toplevel->wlr.handle, activated);
}

void e_foreign_toplevel_set_fullscreen(struct e_foreign_toplevel* foreign_toplevel, bool fullscreen)
{
    assert(foreign_toplevel);

    wlr_foreign_toplevel_handle_v1_set_fullscreen(foreign_toplevel->wlr.handle, fullscreen);
}

void e_foreign_toplevel_destroy(struct e_foreign_toplevel* foreign_toplevel)
{
    assert(foreign_toplevel);

    ext_handle_fini(&foreign_toplevel->ext);
    wlr_handle_fini(&foreign_toplevel->wlr);

    free(foreign_toplevel);
}
