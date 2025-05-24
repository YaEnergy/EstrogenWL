#include "protocols/cosmic-workspace-v1.h"

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include "util/wl_macros.h"

#include "protocols/transactions.h"

#include "cosmic-workspace-unstable-v1-protocol.h"

//REF: https://wayland-book.com/registry/server-side.html

/* workspace manager interface */

static void e_cosmic_workspace_manager_v1_commit(struct wl_client* client, struct wl_resource* resource)
{
    //TODO: implement e_cosmic_workspace_manager_v1_commit
}

// Clients no longer wants to receive events.
static void e_cosmic_workspace_manager_v1_stop(struct wl_client* client, struct wl_resource* resource)
{
    struct e_cosmic_workspace_manager_v1* manager = wl_resource_get_user_data(resource);
    wl_signal_emit_mutable(&manager->events.stop, NULL);

    zcosmic_workspace_manager_v1_send_finished(resource);
    wl_resource_destroy(resource);
}

static const struct zcosmic_workspace_manager_v1_interface workspace_manager_interface = {
    .commit = e_cosmic_workspace_manager_v1_commit,
    .stop = e_cosmic_workspace_manager_v1_stop
};

/* workspace manager */

static void e_cosmic_workspace_manager_v1_resource_destroy(struct wl_resource* resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

// Client wants to bind to manager's global.
static void e_cosmic_workspace_manager_v1_bind(struct wl_client* client, void* data, uint32_t version, uint32_t id)
{
    struct e_cosmic_workspace_manager_v1* manager = data;

    struct wl_resource* resource = wl_resource_create(client, &zcosmic_workspace_manager_v1_interface, zcosmic_workspace_handle_v1_interface.version, id);

    wl_resource_set_implementation(resource, &workspace_manager_interface, manager, e_cosmic_workspace_manager_v1_resource_destroy);
    
    wl_list_insert(&manager->resources, wl_resource_get_link(resource));
}

static void e_cosmic_workspace_manager_v1_display_destroy(struct wl_listener* listener, void* data)
{
    struct e_cosmic_workspace_manager_v1* manager = wl_container_of(listener, manager, listeners.display_destroy);

    wl_signal_emit_mutable(&manager->events.destroy, NULL);

    //TODO: destroy all workspaces?
    //TODO: destroy all groups?
    //TODO: check signal listeners

    e_trans_session_clear(&manager->trans_session);

    SIGNAL_DISCONNECT(manager->listeners.display_destroy);

    wl_global_remove(manager->global);
    wl_global_destroy(manager->global);
}

// Returns NULL on fail.
struct e_cosmic_workspace_manager_v1* e_cosmic_workspace_manager_v1_create(struct wl_display* display, uint32_t version)
{
    assert(version <= COSMIC_WORKSPACE_V1_VERSION);

    if (display == NULL)
        return NULL;

    struct e_cosmic_workspace_manager_v1* manager = calloc(1, sizeof(*manager));

    if (manager == NULL)
        return NULL;

    manager->global = wl_global_create(display, &zcosmic_workspace_manager_v1_interface, version, manager, e_cosmic_workspace_manager_v1_bind);

    if (manager->global == NULL)
    {
        free(manager);
        return NULL;
    }

    wl_list_init(&manager->groups);
    e_trans_session_init(&manager->trans_session);

    //init events

    wl_signal_init(&manager->events.commit);
    wl_signal_init(&manager->events.stop);
    wl_signal_init(&manager->events.destroy);

    //destroy automatically on destruction of display
    manager->listeners.display_destroy.notify = e_cosmic_workspace_manager_v1_display_destroy;
    wl_display_add_destroy_listener(display, &manager->listeners.display_destroy);
    

    return manager;
}