#include "desktop/foreign_toplevel.h"

#include <assert.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>

#include "util/wl_macros.h"

#include "desktop/views/view.h"

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

static void wlr_handle_handle_destroy(struct wl_listener* listener, void* data)
{
    struct e_wlr_foreign_toplevel* wlr_handle = wl_container_of(listener, wlr_handle, destroy);

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

    //TODO: initial state

    return foreign_toplevel;
}

void e_foreign_toplevel_destroy(struct e_foreign_toplevel* foreign_toplevel)
{
    assert(foreign_toplevel);

    ext_handle_fini(&foreign_toplevel->ext);
    wlr_handle_fini(&foreign_toplevel->wlr);

    free(foreign_toplevel);
}
