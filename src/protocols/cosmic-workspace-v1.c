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

//REF 1: https://wayland-book.com/registry/server-side.html
//REF 2: https://wayland.app/protocols/cosmic-workspace-unstable-v1
//REF 3: labwc

/* workspace group interface */

static void e_cosmic_workspace_group_v1_create_workspace(struct wl_client* client, struct wl_resource* resource, const char* workspace_name)
{
    //TODO: implement e_cosmic_workspace_group_v1_create_workspace
}

static void e_cosmic_workspace_group_v1_destroy(struct wl_client* client, struct wl_resource* resource)
{
    //TODO: Implement e_cosmic_workspace_group_v1_destroy, destroy group and its workspaces.
}

static const struct zcosmic_workspace_group_handle_v1_interface workspace_group_interface = {
    .create_workspace = e_cosmic_workspace_group_v1_create_workspace,
    .destroy = e_cosmic_workspace_group_v1_destroy
};

/* workspace group */

static void e_cosmic_workspace_group_v1_resource_destroy(struct wl_resource* resource)
{
    //TODO: clean up resource

    wl_list_remove(wl_resource_get_link(resource));
}

static void e_cosmic_workspace_group_v1_create_resource(struct e_cosmic_workspace_group_v1* group, struct wl_resource* manager_resource)
{
    struct wl_client* client = wl_resource_get_client(manager_resource);
    uint32_t id = wl_resource_get_id(manager_resource);

    struct wl_resource* group_resource = wl_resource_create(client, &zcosmic_workspace_group_handle_v1_interface, COSMIC_WORKSPACE_V1_VERSION, id);

    if (group_resource == NULL)
    {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(group_resource, &workspace_group_interface, group, e_cosmic_workspace_group_v1_resource_destroy);

    wl_list_insert(&group->resources, wl_resource_get_link(group_resource));

    zcosmic_workspace_manager_v1_send_workspace_group(manager_resource, group_resource);

    zcosmic_workspace_manager_v1_send_done(manager_resource);
}

// Returns NULL on fail.
struct e_cosmic_workspace_group_v1* e_cosmic_workspace_group_v1_create(struct e_cosmic_workspace_manager_v1* manager)
{
    //TODO: e_cosmic_workspace_group_v1_create
    struct e_cosmic_workspace_group_v1* group = calloc(1, sizeof(*group));

    if (group == NULL)
        return NULL;

    group->manager = manager;

    wl_list_init(&group->outputs);
    wl_list_init(&group->workspaces);
    wl_list_init(&group->resources);

    wl_array_init(&group->capabilities);

    wl_signal_init(&group->events.request_create_workspace);
    wl_signal_init(&group->events.destroy);

    wl_list_insert(&manager->groups, &group->link);

    //create group resource for each client that binded to manager

    struct wl_resource* manager_resource = NULL;
    struct wl_resource* tmp;

    wl_list_for_each_safe(manager_resource, tmp, &manager->resources, link)
    {
        e_cosmic_workspace_group_v1_create_resource(group, manager_resource);
    }

    return group;
}

// Set capabilities of workspace group, where capabilities contains bitmasks of enum e_cosmic_workspace_group_capability.
void e_cosmic_workspace_group_v1_set_capabilities(struct e_cosmic_workspace_group_v1* group, struct wl_array* capabilities)
{
    //TODO: implement e_cosmic_workspace_group_v1_set_capabilities
}

// Assign output to workspace group.
void e_cosmic_workspace_group_v1_output_enter(struct e_cosmic_workspace_group_v1* group, struct wlr_output* output)
{
    //TODO: implement e_cosmic_workspace_group_v1_output_enter
}

// Remove output from workspace group.
void e_cosmic_workspace_group_v1_output_leave(struct e_cosmic_workspace_group_v1* group, struct wlr_output* output)
{
    //TODO: implement e_cosmic_workspace_group_v1_output_leave
}

// Destroy workspace group and its workspaces.
void e_cosmic_workspace_group_v1_remove(struct e_cosmic_workspace_group_v1* group)
{
    //TODO: implement e_cosmic_workspace_group_v1_destroy
}

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