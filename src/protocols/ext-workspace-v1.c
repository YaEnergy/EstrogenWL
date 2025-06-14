#include "protocols/ext-workspace-v1.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include <wlr/types/wlr_output.h>

#include "util/wl_macros.h"

#include "protocols/transactions.h"

#include "ext-workspace-v1-protocol.h"

//REF 1: https://wayland-book.com/registry/server-side.html
//REF 2: https://wayland.app/protocols/ext-workspace-v1
//REF 3: labwc

enum manager_op_type
{
    MANAGER_GROUP_CREATE_WORKSPACE, //struct group_create_workspace_event*

    MANAGER_WORKSPACE_ACTIVATE,
    MANAGER_WORKSPACE_DEACTIVATE,
    MANAGER_WORKSPACE_ASSIGN, //struct workspace_assign_event*
    MANAGER_WORKSPACE_REMOVE,
};

// Duplicates given null-terminated string.
// Returned string must be freed, returns NULL on fail.
static char* e_strdup(const char* string)
{
    size_t len = strlen(string);
    char* copy = calloc(len + 1, sizeof(*copy));

    if (copy == NULL)
        return NULL;

    strncpy(copy, string, len);
    copy[len] = '\0'; //null-terminator
    
    return copy;
}

static void wl_array_append_uint32_t(struct wl_array* array, uint32_t num)
{
    uint32_t* start = wl_array_add(array, sizeof(uint32_t));

    if (start != NULL)
        *start = num;
}

/* workspace manager schedule */

static void e_ext_workspace_manager_schedule_done_event(struct e_ext_workspace_manager* manager);

/* workspace interface */

/* workspace */

/* workspace group interface */

/* workspace group */

/* workspace manager done schedule */

static void manager_idle_send_done_event(void* data)
{
    struct e_ext_workspace_manager* manager = data;

    //TODO: send pending workspace state

    struct wl_resource* resource;
    wl_list_for_each(resource, &manager->resources, link)
    {
        ext_workspace_manager_v1_send_done(resource);
    }

    manager->done_idle_event = NULL;
}

static void e_ext_workspace_manager_schedule_done_event(struct e_ext_workspace_manager* manager)
{
    if (manager == NULL || manager->event_loop == NULL)
        return;

    //already scheduled
    if (manager->done_idle_event != NULL)
        return;

    manager->done_idle_event = wl_event_loop_add_idle(manager->event_loop, manager_idle_send_done_event, manager);
}

/* workspace manager interface */

// Handle all requested operations at once.
static void e_ext_workspace_manager_commit(struct wl_client* client, struct wl_resource* resource)
{
    //TODO: e_cosmic_workspace_manager_commit
}

// Clients no longer wants to receive events.
static void e_ext_workspace_manager_stop(struct wl_client* client, struct wl_resource* resource)
{
    ext_workspace_manager_v1_send_finished(resource);
    wl_resource_destroy(resource);
}

static const struct ext_workspace_manager_v1_interface workspace_manager_interface = {
    .commit = e_ext_workspace_manager_commit,
    .stop = e_ext_workspace_manager_stop
};


/* workspace manager */

static void e_ext_workspace_manager_resource_destroy(struct wl_resource* resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

// Client wants to bind to manager's global.
static void e_ext_workspace_manager_bind(struct wl_client* client, void* data, uint32_t version, uint32_t id)
{
    struct e_ext_workspace_manager* manager = data;

    struct wl_resource* resource = wl_resource_create(client, &ext_workspace_manager_v1_interface, version, id);

    wl_resource_set_implementation(resource, &workspace_manager_interface, manager, e_ext_workspace_manager_resource_destroy);
    
    wl_list_insert(&manager->resources, wl_resource_get_link(resource));

    //TODO: create resources for every group + send events

    //TODO: create resources for every workspace + send events

    ext_workspace_manager_v1_send_done(resource);
}

static void e_ext_workspace_manager_display_destroy(struct wl_listener* listener, void* data)
{
    struct e_ext_workspace_manager* manager = wl_container_of(listener, manager, listeners.display_destroy);

    wl_signal_emit_mutable(&manager->events.destroy, NULL);

    //destroy all groups
    struct e_ext_workspace_group* group;
    struct e_ext_workspace_group* tmp_group;
    wl_list_for_each_safe(group, tmp_group, &manager->groups, link)
    {
        //TODO: e_ext_workspace_group_remove(group);
    }

    //destroy all workspaces
    struct e_ext_workspace* workspace;
    struct e_ext_workspace* tmp_workspace;
    wl_list_for_each_safe(workspace, tmp_workspace, &manager->workspaces, manager_link)
    {
        //TODO: e_ext_workspace_remove(group);
    }

    e_trans_session_clear(&manager->trans_session);

    SIGNAL_DISCONNECT(manager->listeners.display_destroy);

    wl_global_remove(manager->global);
    wl_global_destroy(manager->global);

    free(manager);
}

// Returns NULL on fail.
struct e_ext_workspace_manager* e_ext_workspace_manager_create(struct wl_display* display, uint32_t version, uint32_t capabilities)
{
    assert(version <= EXT_WORKSPACE_V1_VERSION);

    if (display == NULL)
        return NULL;

    struct e_ext_workspace_manager* manager = calloc(1, sizeof(*manager));

    if (manager == NULL)
        return NULL;

    manager->event_loop = wl_display_get_event_loop(display);
    manager->done_idle_event = NULL;
    manager->capabilities = capabilities;
    manager->global = wl_global_create(display, &ext_workspace_manager_v1_interface, version, manager, e_ext_workspace_manager_bind);

    if (manager->global == NULL)
    {
        free(manager);
        return NULL;
    }

    wl_list_init(&manager->resources);
    wl_list_init(&manager->groups);
    wl_list_init(&manager->workspaces);
    e_trans_session_init(&manager->trans_session);

    //init events

    wl_signal_init(&manager->events.destroy);

    //destroy automatically on destruction of display
    manager->listeners.display_destroy.notify = e_ext_workspace_manager_display_destroy;
    wl_display_add_destroy_listener(display, &manager->listeners.display_destroy);
    
    return manager;
}