#include "protocols/cosmic-workspace-v1.h"

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

#include "cosmic-workspace-unstable-v1-protocol.h"

//REF 1: https://wayland-book.com/registry/server-side.html
//REF 2: https://wayland.app/protocols/cosmic-workspace-unstable-v1
//REF 3: labwc

enum manager_op_type
{
    MANAGER_GROUP_CREATE_WORKSPACE, //struct group_create_workspace_event*

    MANAGER_WORKSPACE_ACTIVATE,
    MANAGER_WORKSPACE_DEACTIVATE,
    MANAGER_WORKSPACE_REMOVE
    //MANAGER_WORKSPACE_RENAME (since minor version 2)
    //MANAGER_WORKSPACE_SET_TILING_STATE (since minor version 2)
};

// Output assigned to a group.
struct group_output
{
    struct e_cosmic_workspace_group_v1* group;
    struct wlr_output* output;

    struct wl_listener group_destroy;

    struct wl_listener output_bind;
    struct wl_listener output_destroy;

    struct wl_list link; //group::outputs
};

// Duplicates given null-terminated string.
// Returned string must be freed, returns NULL on fail.
char* e_strdup(const char* string)
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

static void e_cosmic_workspace_manager_v1_schedule_done_event(struct e_cosmic_workspace_manager_v1* manager);

/* group output */

static void group_output_destroy(struct group_output* group_output)
{
    assert(group_output);

    if (group_output == NULL)
        return;

    //send output enter using every group resource to every output resource
    struct wl_resource* group_resource;
    wl_list_for_each(group_resource, &group_output->group->resources, link)
    {
        struct wl_resource* output_resource;
        wl_list_for_each(output_resource, &group_output->output->resources, link)
        {
            zcosmic_workspace_group_handle_v1_send_output_leave(group_resource, output_resource);
        }    
    }

    SIGNAL_DISCONNECT(group_output->group_destroy);

    SIGNAL_DISCONNECT(group_output->output_bind);
    SIGNAL_DISCONNECT(group_output->output_destroy);

    wl_list_remove(&group_output->link);

    free(group_output);
}

static void group_output_group_destroy(struct wl_listener* listener, void* data)
{
    struct group_output* group_output = wl_container_of(listener, group_output, group_destroy);

    group_output_destroy(group_output);
}

// New client bound to output that is assigned to a group, we must send the output enter event for its new resource.
static void group_output_output_bind(struct wl_listener* listener, void* data)
{
    struct group_output* group_output = wl_container_of(listener, group_output, output_bind);

    struct wlr_output_event_bind* event = data;

    //client that bound to output
    struct wl_client* client = wl_resource_get_client(event->resource);

    bool event_sent = false;

    //TODO: Can there be multiple resources for the same client? I don't think there can be, but just in case it can happen, I won't break the loop.

    //send output enter event to group resources for this client.
    struct wl_resource* group_resource;
    wl_list_for_each(group_resource, &group_output->group->resources, link)
    {
        if (wl_resource_get_client(group_resource) == client)
        {
            zcosmic_workspace_group_handle_v1_send_output_enter(group_resource, event->resource);
            event_sent = true;
        }
    }

    if (!event_sent)
        return;

    //if we sent an event, send done event to manager resource for this client
    struct wl_resource* manager_resource;
    wl_list_for_each(manager_resource, &group_output->group->manager->resources, link)
    {
        if (wl_resource_get_client(manager_resource) == client)
            zcosmic_workspace_manager_v1_send_done(manager_resource);
    }
}

static void group_output_output_destroy(struct wl_listener* listener, void* data)
{
    struct group_output* group_output = wl_container_of(listener, group_output, output_destroy);

    group_output_destroy(group_output);
}

// Returns NULL if none.
static struct group_output* group_output_from_wlr_output(struct e_cosmic_workspace_group_v1* group, struct wlr_output* output)
{
    assert(group && output);

    if (group == NULL || output == NULL)
        return NULL;

    struct group_output* group_output;
    wl_list_for_each(group_output, &group->outputs, link)
    {
        if (group_output->output == output)
            return group_output;
    }

    return NULL;
}

/* workspace interface */

static void e_cosmic_workspace_v1_request_activate(struct wl_client* client, struct wl_resource* resource)
{
    struct e_cosmic_workspace_v1* workspace = wl_resource_get_user_data(resource);
    e_trans_session_add_op(&workspace->group->manager->trans_session, workspace, MANAGER_WORKSPACE_ACTIVATE, NULL);
}

static void e_cosmic_workspace_v1_request_deactivate(struct wl_client* client, struct wl_resource* resource)
{
    struct e_cosmic_workspace_v1* workspace = wl_resource_get_user_data(resource);
    e_trans_session_add_op(&workspace->group->manager->trans_session, workspace, MANAGER_WORKSPACE_DEACTIVATE, NULL);
}

static void e_cosmic_workspace_v1_request_remove(struct wl_client* client, struct wl_resource* resource)
{
    struct e_cosmic_workspace_v1* workspace = wl_resource_get_user_data(resource);
    e_trans_session_add_op(&workspace->group->manager->trans_session, workspace, MANAGER_WORKSPACE_REMOVE, NULL);
}

// Client does not want workspace object anymore.
static void e_cosmic_workspace_v1_destroy(struct wl_client* client, struct wl_resource* resource)
{
    wl_resource_destroy(resource);
}

static const struct zcosmic_workspace_handle_v1_interface workspace_interface = {
    .activate = e_cosmic_workspace_v1_request_activate,
    .deactivate = e_cosmic_workspace_v1_request_deactivate,
    .remove = e_cosmic_workspace_v1_request_remove,
    .destroy = e_cosmic_workspace_v1_destroy
};

/* workspace */

static void workspace_state_to_wl_array(uint32_t workspace_state, struct wl_array* array)
{
    if (array == NULL)
        return;

    if (workspace_state & E_COSMIC_WORKSPACE_STATE_ACTIVE)
        wl_array_append_uint32_t(array, (uint32_t)ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_ACTIVE);

    if (workspace_state & E_COSMIC_WORKSPACE_STATE_URGENT)
        wl_array_append_uint32_t(array, (uint32_t)ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_URGENT);

    if (workspace_state & E_COSMIC_WORKSPACE_STATE_HIDDEN)
        wl_array_append_uint32_t(array, (uint32_t)ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_HIDDEN);
}

// Sends state of workspace to all its resources if resource is NULL.
// If resource is not NULL, only sends state to that resource.
static void e_cosmic_workspace_v1_send_state(struct e_cosmic_workspace_v1* workspace, struct wl_resource* resource)
{
    assert(workspace);

    if (workspace == NULL)
        return;

    struct wl_array state_array;
    wl_array_init(&state_array);

    workspace_state_to_wl_array(workspace->state, &state_array);

    if (resource != NULL)
    {
        zcosmic_workspace_handle_v1_send_state(resource, &state_array);
    }
    else
    {
        struct wl_resource* workspace_resource;
        wl_list_for_each(workspace_resource, &workspace->resources, link)
        {
            zcosmic_workspace_handle_v1_send_state(workspace_resource, &state_array);
        }
    }

    wl_array_release(&state_array);
}

static void e_cosmic_workspace_v1_resource_destroy(struct wl_resource* resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

// Returns NULL on fail.
static struct wl_resource* e_cosmic_workspace_v1_create_resource(struct e_cosmic_workspace_v1* workspace, struct wl_resource* group_resource)
{
    struct wl_client* client = wl_resource_get_client(group_resource);

    struct wl_resource* workspace_resource = wl_resource_create(client, &zcosmic_workspace_handle_v1_interface, wl_resource_get_version(group_resource), 0);

    if (workspace_resource == NULL)
    {
        wl_client_post_no_memory(client);
        return NULL;
    }

    wl_resource_set_implementation(workspace_resource, &workspace_interface, workspace, e_cosmic_workspace_v1_resource_destroy);

    wl_list_insert(&workspace->resources, wl_resource_get_link(workspace_resource));

    zcosmic_workspace_group_handle_v1_send_workspace(group_resource, workspace_resource);
    zcosmic_workspace_handle_v1_send_capabilities(workspace_resource, &workspace->capabilities);

    if (workspace->name != NULL)
        zcosmic_workspace_handle_v1_send_name(workspace_resource, workspace->name);
    
    return workspace_resource;
}

static void workspace_init_capabilities(struct e_cosmic_workspace_v1* workspace, uint32_t manager_capabilities)
{
    if (workspace == NULL)
        return;

    wl_array_init(&workspace->capabilities);

    if (manager_capabilities & E_COSMIC_WORKSPACE_CAPABILITY_ACTIVATE)
        wl_array_append_uint32_t(&workspace->capabilities, ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_ACTIVATE);

    if (manager_capabilities & E_COSMIC_WORKSPACE_CAPABILITY_DEACTIVATE)
        wl_array_append_uint32_t(&workspace->capabilities, ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_DEACTIVATE);

    if (manager_capabilities & E_COSMIC_WORKSPACE_CAPABILITY_REMOVE)
        wl_array_append_uint32_t(&workspace->capabilities, ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_REMOVE);
}

// Returns NULL on fail.
struct e_cosmic_workspace_v1* e_cosmic_workspace_v1_create(struct e_cosmic_workspace_group_v1* group)
{
    struct e_cosmic_workspace_v1* workspace = calloc(1, sizeof(*workspace));

    if (workspace == NULL)
        return NULL;

    workspace->name = NULL;
    workspace->group = group;

    wl_array_init(&workspace->capabilities);
    wl_array_init(&workspace->coords);

    wl_list_init(&workspace->resources);

    workspace_init_capabilities(workspace, group->manager->capabilities);

    wl_signal_init(&workspace->events.request_activate);
    wl_signal_init(&workspace->events.request_deactivate);
    wl_signal_init(&workspace->events.request_remove);
    wl_signal_init(&workspace->events.destroy);

    wl_list_append(group->workspaces, &workspace->link);

    //create workspace resource for each client that has binded to manager

    struct wl_resource* group_resource = NULL;
    struct wl_resource* tmp;

    wl_list_for_each_safe(group_resource, tmp, &group->resources, link)
    {
        e_cosmic_workspace_v1_create_resource(workspace, group_resource);
    }

    e_cosmic_workspace_manager_v1_schedule_done_event(group->manager);

    return workspace;
}

// Name is copied.
void e_cosmic_workspace_v1_set_name(struct e_cosmic_workspace_v1* workspace, const char* name)
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
        zcosmic_workspace_handle_v1_send_name(resource, copy);
    }

    e_cosmic_workspace_manager_v1_schedule_done_event(workspace->group->manager);
}

// Set coordinates of the workspace.
void e_cosmic_workspace_v1_set_coords(struct e_cosmic_workspace_v1* workspace, struct wl_array* coords)
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
        zcosmic_workspace_handle_v1_send_coordinates(resource, &workspace->coords);
    }

    e_cosmic_workspace_manager_v1_schedule_done_event(workspace->group->manager);
}

// Set whether or not workspace is in a specific state.
static void workspace_set_state(struct e_cosmic_workspace_v1* workspace, enum e_cosmic_workspace_state state, bool enabled)
{
    assert(workspace);

    if (workspace == NULL)
        return;

    if (enabled)
        workspace->pending_state |= state;
    else
        workspace->pending_state &= ~state;

    e_cosmic_workspace_manager_v1_schedule_done_event(workspace->group->manager);
}

// Set whether or not workspace is active.
void e_cosmic_workspace_v1_set_active(struct e_cosmic_workspace_v1* workspace, bool active)
{
    workspace_set_state(workspace, E_COSMIC_WORKSPACE_STATE_ACTIVE, active);
}

// Set whether or not workspace wants attention.
void e_cosmic_workspace_v1_set_urgent(struct e_cosmic_workspace_v1* workspace, bool urgent)
{
    workspace_set_state(workspace, E_COSMIC_WORKSPACE_STATE_URGENT, urgent);
}

// Set whether or not workspace is hidden.
void e_cosmic_workspace_v1_set_hidden(struct e_cosmic_workspace_v1* workspace, bool hidden)
{
    workspace_set_state(workspace, E_COSMIC_WORKSPACE_STATE_HIDDEN, hidden);
}

// Destroys workspace.
void e_cosmic_workspace_v1_remove(struct e_cosmic_workspace_v1* workspace)
{
    assert(workspace);

    if (workspace == NULL)
        return;

    wl_signal_emit_mutable(&workspace->events.destroy, NULL);

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
        zcosmic_workspace_handle_v1_send_remove(resource);

        wl_list_remove(wl_resource_get_link(resource)); //remove from resources

        wl_resource_set_user_data(resource, NULL);
        wl_list_init(wl_resource_get_link(resource)); //resource will destroy itself later, and remove itself again
    }

    wl_list_remove(&workspace->link);

    wl_array_release(&workspace->capabilities);
    wl_array_release(&workspace->coords);

    if (workspace->name != NULL)
        free(workspace->name);

    free(workspace);
}

/* workspace group interface */

struct group_create_workspace_event
{
    char* name;

    struct wl_listener destroy;
};

void group_create_workspace_event_destroy(struct wl_listener* listener, void* data)
{
    struct group_create_workspace_event* event = wl_container_of(listener, event, destroy);

    SIGNAL_DISCONNECT(event->destroy);

    free(event->name);

    free(event);
}

static void e_cosmic_workspace_group_v1_create_workspace(struct wl_client* client, struct wl_resource* resource, const char* workspace_name)
{
    struct e_cosmic_workspace_group_v1* group = wl_resource_get_user_data(resource);

    struct group_create_workspace_event* event = calloc(1, sizeof(*event));

    if (event == NULL)
    {
        wl_client_post_no_memory(client);
        return;
    }

    event->name = e_strdup(workspace_name);

    if (event->name == NULL)
    {
        free(event);
        wl_client_post_no_memory(client);
        return;
    }

    struct e_trans_op* operation = e_trans_session_add_op(&group->manager->trans_session, group, MANAGER_GROUP_CREATE_WORKSPACE, event);

    if (operation == NULL)
    {
        free(event->name);
        free(event);
        wl_client_post_no_memory(client);
        return;
    }

    SIGNAL_CONNECT(operation->destroy, event->destroy, group_create_workspace_event_destroy);
}

// Client does not want group object anymore.
static void e_cosmic_workspace_group_v1_destroy(struct wl_client* client, struct wl_resource* resource)
{
    wl_resource_destroy(resource);
}

static const struct zcosmic_workspace_group_handle_v1_interface workspace_group_interface = {
    .create_workspace = e_cosmic_workspace_group_v1_create_workspace,
    .destroy = e_cosmic_workspace_group_v1_destroy
};

/* workspace group */

static void e_cosmic_workspace_group_v1_resource_destroy(struct wl_resource* resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

// Returns NULL on fail.
static struct wl_resource* e_cosmic_workspace_group_v1_create_resource(struct e_cosmic_workspace_group_v1* group, struct wl_resource* manager_resource)
{
    struct wl_client* client = wl_resource_get_client(manager_resource);

    struct wl_resource* group_resource = wl_resource_create(client, &zcosmic_workspace_group_handle_v1_interface, wl_resource_get_version(manager_resource), 0);

    if (group_resource == NULL)
    {
        wl_client_post_no_memory(client);
        return NULL;
    }

    wl_resource_set_implementation(group_resource, &workspace_group_interface, group, e_cosmic_workspace_group_v1_resource_destroy);

    wl_list_insert(&group->resources, wl_resource_get_link(group_resource));

    zcosmic_workspace_manager_v1_send_workspace_group(manager_resource, group_resource);
    zcosmic_workspace_group_handle_v1_send_capabilities(group_resource, &group->capabilities);

    return group_resource;
}

static void group_init_capabilities(struct e_cosmic_workspace_group_v1* group, uint32_t manager_capabilities)
{
    if (group == NULL)
        return;

    wl_array_init(&group->capabilities);

    if (manager_capabilities & E_COSMIC_WORKSPACE_GROUP_CAPABILITY_CREATE_WORKSPACE)
        wl_array_append_uint32_t(&group->capabilities, ZCOSMIC_WORKSPACE_GROUP_HANDLE_V1_ZCOSMIC_WORKSPACE_GROUP_CAPABILITIES_V1_CREATE_WORKSPACE);
}

// Returns NULL on fail.
struct e_cosmic_workspace_group_v1* e_cosmic_workspace_group_v1_create(struct e_cosmic_workspace_manager_v1* manager)
{
    struct e_cosmic_workspace_group_v1* group = calloc(1, sizeof(*group));

    if (group == NULL)
        return NULL;

    group->manager = manager;

    wl_list_init(&group->outputs);
    wl_list_init(&group->workspaces);
    wl_list_init(&group->resources);

    group_init_capabilities(group, manager->capabilities);

    wl_signal_init(&group->events.request_create_workspace);
    wl_signal_init(&group->events.destroy);

    wl_list_append(manager->groups, &group->link);

    //create group resource for each client that binded to manager

    struct wl_resource* manager_resource = NULL;
    struct wl_resource* tmp;

    wl_list_for_each_safe(manager_resource, tmp, &manager->resources, link)
    {
        e_cosmic_workspace_group_v1_create_resource(group, manager_resource);
    }

    e_cosmic_workspace_manager_v1_schedule_done_event(group->manager);

    return group;
}

// Assign output to workspace group.
void e_cosmic_workspace_group_v1_output_enter(struct e_cosmic_workspace_group_v1* group, struct wlr_output* output)
{
    assert(group && output);

    if (group == NULL || output == NULL)
        return;

    struct group_output* group_output = calloc(1, sizeof(*group_output));

    if (group_output == NULL)
        return;

    group_output->group = group;
    group_output->output = output;

    SIGNAL_CONNECT(group->events.destroy, group_output->group_destroy, group_output_group_destroy);
    
    SIGNAL_CONNECT(output->events.bind, group_output->output_bind, group_output_output_bind);
    SIGNAL_CONNECT(output->events.destroy, group_output->output_destroy, group_output_output_destroy);

    wl_list_insert(&group->outputs, &group_output->link);

    //send output enter using every group resource to every output resource
    struct wl_resource* group_resource;
    wl_list_for_each(group_resource, &group->resources, link)
    {
        struct wl_resource* output_resource;
        wl_list_for_each(output_resource, &output->resources, link)
        {
            zcosmic_workspace_group_handle_v1_send_output_enter(group_resource, output_resource);
        }    
    }

    e_cosmic_workspace_manager_v1_schedule_done_event(group->manager);
}

// Remove output from workspace group.
void e_cosmic_workspace_group_v1_output_leave(struct e_cosmic_workspace_group_v1* group, struct wlr_output* output)
{
    assert(group && output);

    if (group == NULL || output == NULL)
        return;

    struct group_output* group_output = group_output_from_wlr_output(group, output);
    group_output_destroy(group_output);
}

// Destroy workspace group and its workspaces.
void e_cosmic_workspace_group_v1_remove(struct e_cosmic_workspace_group_v1* group)
{
    assert(group);

    if (group == NULL)
        return;

    wl_signal_emit_mutable(&group->events.destroy, NULL); //also destroys all group outputs

    //remove all of group's workspaces
    struct e_cosmic_workspace_v1* workspace;
    struct e_cosmic_workspace_v1* tmp_workspace;
    wl_list_for_each_safe(workspace, tmp_workspace, &group->workspaces, link)
    {
        e_cosmic_workspace_v1_remove(workspace);
    }

    //remove transaction ops using this group
    struct e_trans_op* operation;
    struct e_trans_op* tmp_operation;
    e_trans_session_for_each_safe(operation, tmp_operation, &group->manager->trans_session)
    {
        if (operation->src == group)
            e_trans_op_destroy(operation);
    }

    struct wl_resource* resource;
    struct wl_resource* tmp_resource;
    wl_list_for_each_safe(resource, tmp_resource, &group->resources, link)
    {
        zcosmic_workspace_group_handle_v1_send_remove(resource);
        
        wl_list_remove(wl_resource_get_link(resource)); //remove from resources

        wl_resource_set_user_data(resource, NULL);
        wl_list_init(wl_resource_get_link(resource)); //resource will destroy itself later, and remove itself again
    }

    wl_list_remove(&group->link);

    wl_array_release(&group->capabilities);

    free(group);
}

/* workspace manager done schedule */

static void manager_idle_send_done_event(void* data)
{
    struct e_cosmic_workspace_manager_v1* manager = data;

    //send pending workspace state
    struct e_cosmic_workspace_group_v1* group;
    wl_list_for_each(group, &manager->groups, link)
    {
        struct e_cosmic_workspace_v1* workspace;
        wl_list_for_each(workspace, &group->workspaces, link)
        {
            if (workspace->pending_state != workspace->state)
            {
                workspace->state = workspace->pending_state;
                e_cosmic_workspace_v1_send_state(workspace, NULL);
            }
        }   
    }

    struct wl_resource* resource;
    wl_list_for_each(resource, &manager->resources, link)
    {
        zcosmic_workspace_manager_v1_send_done(resource);
    }

    manager->done_idle_event = NULL;
}

static void e_cosmic_workspace_manager_v1_schedule_done_event(struct e_cosmic_workspace_manager_v1* manager)
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
static void e_cosmic_workspace_manager_v1_commit(struct wl_client* client, struct wl_resource* resource)
{
    struct e_cosmic_workspace_manager_v1* manager = wl_resource_get_user_data(resource);

    struct e_cosmic_workspace_group_v1* group;
    struct e_cosmic_workspace_v1* workspace;

    struct e_trans_op* operation;
    struct e_trans_op* tmp;
    e_trans_session_for_each_safe(operation, tmp, &manager->trans_session)
    {
        switch(operation->type)
        {
            case MANAGER_GROUP_CREATE_WORKSPACE:
                group = operation->src;
                struct group_create_workspace_event* event = operation->data;
                wl_signal_emit_mutable(&group->events.request_create_workspace, event->name);
                break;
            case MANAGER_WORKSPACE_ACTIVATE:
                workspace = operation->src;
                wl_signal_emit_mutable(&workspace->events.request_activate, NULL);
                break;
            case MANAGER_WORKSPACE_DEACTIVATE:
                workspace = operation->src;
                wl_signal_emit_mutable(&workspace->events.request_deactivate, NULL);
                break;
            case MANAGER_WORKSPACE_REMOVE:
                workspace = operation->src;
                wl_signal_emit_mutable(&workspace->events.request_remove, NULL);
                break;
        }

        e_trans_op_destroy(operation);
    }

    e_cosmic_workspace_manager_v1_schedule_done_event(manager);
}

// Clients no longer wants to receive events.
static void e_cosmic_workspace_manager_v1_stop(struct wl_client* client, struct wl_resource* resource)
{
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

    struct wl_resource* resource = wl_resource_create(client, &zcosmic_workspace_manager_v1_interface, version, id);

    wl_resource_set_implementation(resource, &workspace_manager_interface, manager, e_cosmic_workspace_manager_v1_resource_destroy);
    
    wl_list_insert(&manager->resources, wl_resource_get_link(resource));

    //create resources for every group & workspace for this client
    if (!wl_list_empty(&manager->groups))
    {
        struct e_cosmic_workspace_group_v1* group;
        wl_list_for_each(group, &manager->groups, link)
        {
            e_log_info("create group resource");

            struct wl_resource* group_resource = e_cosmic_workspace_group_v1_create_resource(group, resource);

            if (wl_list_empty(&group->workspaces))
                continue;

            struct e_cosmic_workspace_v1* workspace;
            wl_list_for_each(workspace, &group->workspaces, link)
            {
                e_log_info("create workspace resource");
                
                struct wl_resource* workspace_resource = e_cosmic_workspace_v1_create_resource(workspace, group_resource);
                zcosmic_workspace_handle_v1_send_coordinates(workspace_resource, &workspace->coords);
                e_cosmic_workspace_v1_send_state(workspace, workspace_resource);
            }
        }
    }

    zcosmic_workspace_manager_v1_send_done(resource);
}

static void e_cosmic_workspace_manager_v1_display_destroy(struct wl_listener* listener, void* data)
{
    struct e_cosmic_workspace_manager_v1* manager = wl_container_of(listener, manager, listeners.display_destroy);

    wl_signal_emit_mutable(&manager->events.destroy, NULL);

    //destroy all groups, including their workspaces
    struct e_cosmic_workspace_group_v1* group;
    struct e_cosmic_workspace_group_v1* tmp;
    wl_list_for_each_safe(group, tmp, &manager->groups, link)
    {
        e_cosmic_workspace_group_v1_remove(group);
    }

    e_trans_session_clear(&manager->trans_session);

    SIGNAL_DISCONNECT(manager->listeners.display_destroy);

    wl_global_remove(manager->global);
    wl_global_destroy(manager->global);

    free(manager);
}

// Returns NULL on fail.
struct e_cosmic_workspace_manager_v1* e_cosmic_workspace_manager_v1_create(struct wl_display* display, uint32_t version, uint32_t capabilities)
{
    assert(version <= COSMIC_WORKSPACE_V1_VERSION);

    if (display == NULL)
        return NULL;

    struct e_cosmic_workspace_manager_v1* manager = calloc(1, sizeof(*manager));

    if (manager == NULL)
        return NULL;

    manager->event_loop = wl_display_get_event_loop(display);
    manager->done_idle_event = NULL;
    manager->capabilities = capabilities;
    manager->global = wl_global_create(display, &zcosmic_workspace_manager_v1_interface, version, manager, e_cosmic_workspace_manager_v1_bind);

    if (manager->global == NULL)
    {
        free(manager);
        return NULL;
    }

    wl_list_init(&manager->resources);
    wl_list_init(&manager->groups);
    e_trans_session_init(&manager->trans_session);

    //init events

    wl_signal_init(&manager->events.destroy);

    //destroy automatically on destruction of display
    manager->listeners.display_destroy.notify = e_cosmic_workspace_manager_v1_display_destroy;
    wl_display_add_destroy_listener(display, &manager->listeners.display_destroy);
    
    return manager;
}