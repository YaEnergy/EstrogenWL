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

/* workspace manager schedule */

static void e_ext_workspace_manager_schedule_done_event(struct e_ext_workspace_manager* manager);

/* workspace interface */

static void e_ext_workspace_request_activate(struct wl_client* client, struct wl_resource* resource)
{
    struct e_ext_workspace* workspace = wl_resource_get_user_data(resource);
    e_trans_session_add_op(&workspace->manager->trans_session, workspace, MANAGER_WORKSPACE_ACTIVATE, NULL);
}

static void e_ext_workspace_request_deactivate(struct wl_client* client, struct wl_resource* resource)
{
    struct e_ext_workspace* workspace = wl_resource_get_user_data(resource);
    e_trans_session_add_op(&workspace->manager->trans_session, workspace, MANAGER_WORKSPACE_DEACTIVATE, NULL);
}

static void e_ext_workspace_request_assign(struct wl_client* client, struct wl_resource* resource, struct wl_resource* group_resource)
{
    //TODO: e_ext_workspace_request_assign
}

static void e_ext_workspace_request_remove(struct wl_client* client, struct wl_resource* resource)
{
    struct e_ext_workspace* workspace = wl_resource_get_user_data(resource);
    e_trans_session_add_op(&workspace->manager->trans_session, workspace, MANAGER_WORKSPACE_REMOVE, NULL);
}

// Client does not want workspace object anymore.
static void e_ext_workspace_destroy(struct wl_client* client, struct wl_resource* resource)
{
    wl_resource_destroy(resource);
}

static const struct ext_workspace_handle_v1_interface workspace_interface = {
    .activate = e_ext_workspace_request_activate,
    .deactivate = e_ext_workspace_request_deactivate,
    .assign = e_ext_workspace_request_assign,
    .remove = e_ext_workspace_request_remove,
    .destroy = e_ext_workspace_destroy
};

/* workspace */

// Sends state of workspace to all its resources if resource is NULL.
// If resource is not NULL, only sends state to that resource.
static void workspace_send_state(struct e_ext_workspace* workspace, struct wl_resource* resource)
{
    assert(workspace);

    if (workspace == NULL)
        return;

    uint32_t state = 0;

    if (workspace->state & E_EXT_WORKSPACE_STATE_ACTIVE)
        state |= EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE;

    if (workspace->state & E_EXT_WORKSPACE_STATE_URGENT)
        state |= EXT_WORKSPACE_HANDLE_V1_STATE_URGENT;

    if (workspace->state & E_EXT_WORKSPACE_STATE_HIDDEN)
        state |= EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN;

    if (resource != NULL)
    {
        ext_workspace_handle_v1_send_state(resource, state);
    }
    else
    {
        struct wl_resource* workspace_resource;
        wl_list_for_each(workspace_resource, &workspace->resources, link)
        {
            ext_workspace_handle_v1_send_state(workspace_resource, state);
        }
    }
}

static void workspace_resource_send_capabilities(struct e_ext_workspace* workspace, struct wl_resource* resource)
{
    assert(workspace && resource);

    if (workspace == NULL || resource == NULL)
        return;

    uint32_t manager_capabilities = workspace->manager->capabilities;
    uint32_t workspace_capabilities = 0;

    if (manager_capabilities & E_EXT_WORKSPACE_CAPABILITY_ACTIVATE)
        workspace_capabilities |= EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE;

    if (manager_capabilities & E_EXT_WORKSPACE_CAPABILITY_DEACTIVATE)
        workspace_capabilities |= EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_DEACTIVATE;

    if (manager_capabilities & E_EXT_WORKSPACE_CAPABILITY_ASSIGN)
        workspace_capabilities |= EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ASSIGN;

    if (manager_capabilities & E_EXT_WORKSPACE_CAPABILITY_REMOVE)
        workspace_capabilities |= EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_REMOVE;

    ext_workspace_handle_v1_send_capabilities(resource, workspace_capabilities);
}

// Sends workspace's id, name, capabilities and coordinates to resource.
// State is sent separately.
static void workspace_resource_send_init(struct e_ext_workspace* workspace, struct wl_resource* resource)
{
    assert(workspace && resource);

    if (workspace == NULL || resource == NULL)
        return;

    if (workspace->id != NULL)
        ext_workspace_handle_v1_send_id(resource, workspace->id);

    workspace_resource_send_capabilities(workspace, resource);

    if (workspace->coords.size != 0)
        ext_workspace_handle_v1_send_coordinates(resource, &workspace->coords);

    if (workspace->name != NULL)
        ext_workspace_handle_v1_send_name(resource, workspace->name);
}

static void e_ext_workspace_resource_destroy(struct wl_resource* resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

// Returns NULL on fail.
static struct wl_resource* e_ext_workspace_create_resource(struct e_ext_workspace* workspace, struct wl_resource* manager_resource)
{
    struct wl_client* client = wl_resource_get_client(manager_resource);

    struct wl_resource* workspace_resource = wl_resource_create(client, &ext_workspace_handle_v1_interface, wl_resource_get_version(manager_resource), 0);

    if (workspace_resource == NULL)
    {
        wl_client_post_no_memory(client);
        return NULL;
    }

    wl_resource_set_implementation(workspace_resource, &workspace_interface, workspace, e_ext_workspace_resource_destroy);

    wl_list_insert(&workspace->resources, wl_resource_get_link(workspace_resource));
    
    return workspace_resource;
}

// Returns NULL on fail.
struct e_ext_workspace* e_ext_workspace_create(struct e_ext_workspace_manager* manager, const char* id)
{
    struct e_ext_workspace* workspace = calloc(1, sizeof(*workspace));

    if (workspace == NULL)
        return NULL;

    workspace->name = NULL;
    workspace->id = (id != NULL) ? e_strdup(id) : NULL;
    workspace->group = NULL;

    wl_array_init(&workspace->coords);

    wl_list_init(&workspace->resources);

    wl_signal_init(&workspace->events.request_activate);
    wl_signal_init(&workspace->events.request_deactivate);
    wl_signal_init(&workspace->events.request_assign);
    wl_signal_init(&workspace->events.request_remove);
    wl_signal_init(&workspace->events.destroy);

    wl_list_append(manager->workspaces, &workspace->manager_link);

    //create workspace resource for each client that has binded to manager

    struct wl_resource* manager_resource = NULL;
    struct wl_resource* tmp;

    wl_list_for_each_safe(manager_resource, tmp, &manager->resources, link)
    {
        struct wl_resource* workspace_resource = e_ext_workspace_create_resource(workspace, manager_resource);

        if (workspace_resource != NULL)
        {
            ext_workspace_manager_v1_send_workspace(manager_resource, workspace_resource);
            workspace_resource_send_init(workspace, workspace_resource);
        }
    }

    workspace_send_state(workspace, NULL);

    e_ext_workspace_manager_schedule_done_event(manager);

    return workspace;
}

static void group_send_workspace_enter(struct e_ext_workspace_group* group, struct e_ext_workspace* workspace)
{
    assert(group && workspace);

    if (group == NULL || workspace == NULL)
        return;

    struct wl_resource* group_resource;
    wl_list_for_each(group_resource, &group->resources, link)
    {
        struct wl_resource* workspace_resource;
        wl_list_for_each(workspace_resource, &workspace->resources, link)
        {
            ext_workspace_group_handle_v1_send_workspace_enter(group_resource, workspace_resource);
        }
    }
}

static void group_send_workspace_leave(struct e_ext_workspace_group* group, struct e_ext_workspace* workspace)
{
    assert(group && workspace);

    if (group == NULL || workspace == NULL)
        return;

    struct wl_resource* group_resource;
    wl_list_for_each(group_resource, &group->resources, link)
    {
        struct wl_resource* workspace_resource;
        wl_list_for_each(workspace_resource, &workspace->resources, link)
        {
            ext_workspace_group_handle_v1_send_workspace_leave(group_resource, workspace_resource);
        }
    }
}

// Assign workspace to group, a workspace can only ever be assigned to one group at a time.
// Group is allowed to be NULL.
void e_ext_workspace_assign_to_group(struct e_ext_workspace* workspace, struct e_ext_workspace_group* group)
{
    assert(workspace);

    if (workspace == NULL)
        return;

    //if already in group, leave
    if (workspace->group != NULL)
    {
        workspace->group = NULL;
        wl_list_remove(&workspace->group_link);
        group_send_workspace_leave(group, workspace);
    }

    //enter requested group, if any
    if (group != NULL)
    {
        workspace->group = group;
        wl_list_insert(&group->workspaces, &workspace->group_link);
        group_send_workspace_enter(group, workspace);
    }
}

// Name is copied.
void e_ext_workspace_set_name(struct e_ext_workspace* workspace, const char* name)
{
    assert(workspace);

    if (workspace == NULL)
        return;

    char* copy = e_strdup(name);

    if (copy == NULL)
        return;

    if (workspace->name != NULL)
        free(workspace->name);

    workspace->name = copy;

    struct wl_resource* resource;
    wl_list_for_each(resource, &workspace->resources, link)
    {
        ext_workspace_handle_v1_send_name(resource, copy);
    }

    e_ext_workspace_manager_schedule_done_event(workspace->manager);
}

// Set coordinates of the workspace.
void e_ext_workspace_set_coords(struct e_ext_workspace* workspace, struct wl_array* coords)
{
    assert(workspace && coords);

    if (workspace == NULL || coords == NULL)
        return;

    wl_array_release(&workspace->coords);

    wl_array_init(&workspace->coords);
    wl_array_copy(&workspace->coords, coords);

    struct wl_resource* resource;
    wl_list_for_each(resource, &workspace->resources, link)
    {
        ext_workspace_handle_v1_send_coordinates(resource, &workspace->coords);
    }

    e_ext_workspace_manager_schedule_done_event(workspace->group->manager);
}

// Set whether or not workspace is in a specific state.
static void workspace_set_state(struct e_ext_workspace* workspace, enum e_ext_workspace_state state, bool enabled)
{
    assert(workspace);

    if (workspace == NULL)
        return;

    if (enabled)
        workspace->pending_state |= state;
    else
        workspace->pending_state &= ~state;

    e_ext_workspace_manager_schedule_done_event(workspace->group->manager);
}

// Set whether or not workspace is active.
void e_ext_workspace_set_active(struct e_ext_workspace* workspace, bool active)
{
    workspace_set_state(workspace, E_EXT_WORKSPACE_STATE_ACTIVE, active);
}

// Set whether or not workspace wants attention.
void e_ext_workspace_set_urgent(struct e_ext_workspace* workspace, bool urgent)
{
    workspace_set_state(workspace, E_EXT_WORKSPACE_STATE_URGENT, urgent);
}

// Set whether or not workspace is hidden.
void e_ext_workspace_set_hidden(struct e_ext_workspace* workspace, bool hidden)
{
    workspace_set_state(workspace, E_EXT_WORKSPACE_STATE_HIDDEN, hidden);
}

// Destroys workspace.
void e_ext_workspace_remove(struct e_ext_workspace* workspace)
{
    assert(workspace);

    if (workspace == NULL)
        return;

    wl_signal_emit_mutable(&workspace->events.destroy, NULL);

    if (workspace->group != NULL)
        e_ext_workspace_assign_to_group(workspace, NULL);

    //remove transaction ops using this workspace
    struct e_trans_op* operation;
    struct e_trans_op* tmp_operation;
    e_trans_session_for_each_safe(operation, tmp_operation, &workspace->group->manager->trans_session)
    {
        if (operation->src == workspace)
            e_trans_op_destroy(operation);
    }

    struct wl_resource* resource;
    struct wl_resource* tmp_resource;
    wl_list_for_each_safe(resource, tmp_resource, &workspace->resources, link)
    {
        ext_workspace_handle_v1_send_removed(resource);

        wl_list_remove(wl_resource_get_link(resource)); //remove from resources

        wl_resource_set_user_data(resource, NULL);
        wl_list_init(wl_resource_get_link(resource)); //resource will destroy itself later, and remove itself again
    }

    wl_list_remove(&workspace->manager_link);

    wl_array_release(&workspace->coords);

    if (workspace->name != NULL)
        free(workspace->name);

    if (workspace->id != NULL)
        free(workspace->id);

    free(workspace);
}

/* workspace group interface */

static void e_ext_workspace_group_create_workspace(struct wl_client* client, struct wl_resource* resource, const char* workspace_name)
{
    //TODO: e_ext_workspace_group_create_workspace
}

// Client does not want group object anymore.
static void e_ext_workspace_group_destroy(struct wl_client* client, struct wl_resource* resource)
{
    wl_resource_destroy(resource);
}

static const struct ext_workspace_group_handle_v1_interface workspace_group_interface = {
    .create_workspace = e_ext_workspace_group_create_workspace,
    .destroy = e_ext_workspace_group_destroy
};

/* workspace group */

// Sends group's capabilities to resource.
static void group_resource_send_capabilities(struct e_ext_workspace_group* group, struct wl_resource* resource)
{
    assert(group && resource);

    if (group == NULL || resource == NULL)
        return;

    uint32_t manager_capabilities = group->manager->capabilities;
    uint32_t group_capabilities = 0;

    if (manager_capabilities & E_EXT_WORKSPACE_GROUP_CAPABILITY_CREATE_WORKSPACE)
        group_capabilities |= EXT_WORKSPACE_GROUP_HANDLE_V1_GROUP_CAPABILITIES_CREATE_WORKSPACE;

    ext_workspace_group_handle_v1_send_capabilities(resource, group_capabilities);
}

static void e_ext_workspace_group_resource_destroy(struct wl_resource* resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

// Returns NULL on fail.
static struct wl_resource* e_ext_workspace_group_create_resource(struct e_ext_workspace_group* group, struct wl_resource* manager_resource)
{
    struct wl_client* client = wl_resource_get_client(manager_resource);

    struct wl_resource* group_resource = wl_resource_create(client, &ext_workspace_group_handle_v1_interface, wl_resource_get_version(manager_resource), 0);

    if (group_resource == NULL)
    {
        wl_client_post_no_memory(client);
        return NULL;
    }

    wl_resource_set_implementation(group_resource, &workspace_group_interface, group, e_ext_workspace_group_resource_destroy);

    wl_list_insert(&group->resources, wl_resource_get_link(group_resource));

    return group_resource;
}

// Returns NULL on fail.
struct e_ext_workspace_group* e_ext_workspace_group_create(struct e_ext_workspace_manager* manager)
{
    struct e_ext_workspace_group* group = calloc(1, sizeof(*group));

    if (group == NULL)
        return NULL;

    group->manager = manager;

    wl_list_init(&group->outputs);
    wl_list_init(&group->workspaces);
    wl_list_init(&group->resources);

    wl_signal_init(&group->events.request_create_workspace);
    wl_signal_init(&group->events.destroy);

    wl_list_append(manager->groups, &group->link);

    //create group resource for each client that binded to manager

    struct wl_resource* manager_resource = NULL;
    struct wl_resource* tmp;

    wl_list_for_each_safe(manager_resource, tmp, &manager->resources, link)
    {
        struct wl_resource* group_resource = e_ext_workspace_group_create_resource(group, manager_resource);

        if (group_resource == NULL)
            continue;

        ext_workspace_manager_v1_send_workspace_group(manager_resource, group_resource);
        group_resource_send_capabilities(group, group_resource);
    }

    e_ext_workspace_manager_schedule_done_event(group->manager);

    return group;
}

/* workspace manager done schedule */

static void manager_idle_send_done_event(void* data)
{
    struct e_ext_workspace_manager* manager = data;

    //send pending workspace state
    struct e_ext_workspace* workspace;
    wl_list_for_each(workspace, &manager->workspaces, manager_link)
    {
        if (workspace->pending_state != workspace->state)
        {
            workspace->state = workspace->pending_state;
            workspace_send_state(workspace, NULL);
        }
    }

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

    struct e_ext_workspace_group* group;
    wl_list_for_each(group, &manager->groups, link)
    {
        struct wl_resource* group_resource = e_ext_workspace_group_create_resource(group, resource);

        if (group_resource == NULL)
            continue;

        ext_workspace_manager_v1_send_workspace_group(resource, group_resource);
        group_resource_send_capabilities(group, group_resource);

        //TODO: group workspaces?
    }

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
        e_ext_workspace_remove(workspace);
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