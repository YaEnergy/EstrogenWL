#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include "protocols/transactions.h"

#define EXT_WORKSPACE_V1_VERSION 1

struct wlr_output;

enum e_ext_workspace_manager_capability
{
    E_EXT_WORKSPACE_GROUP_CAPABILITY_CREATE_WORKSPACE = 1 << 0,
    
    E_EXT_WORKSPACE_CAPABILITY_ACTIVATE = 1 << 1,
    E_EXT_WORKSPACE_CAPABILITY_DEACTIVATE = 1 << 2,
    E_EXT_WORKSPACE_CAPABILITY_ASSIGN = 1 << 3,
    E_EXT_WORKSPACE_CAPABILITY_REMOVE = 1 << 4,
};

struct e_ext_workspace_manager
{
    struct wl_global* global;

    struct wl_event_loop* event_loop;
    struct wl_event_source* done_idle_event;

    struct wl_list groups; //struct e_ext_workspace_group*
    struct wl_list workspaces; //struct e_ext_workspace*

    uint32_t capabilities; //bitmask of enum e_ext_workspace_manager_capability

    // Resource for each client that has binded to manager.
    struct wl_list resources; //struct wl_resource*

    struct e_trans_session trans_session;
    
    struct
    {
        struct wl_signal destroy;
    } events;

    struct 
    {
        // Destroy global when display is destroyed.
        struct wl_listener display_destroy;
    } listeners;
};

struct e_ext_workspace_group
{
    struct e_ext_workspace_manager* manager;

    struct wl_list outputs; //struct group_output*, internal struct
    struct wl_list workspaces; //struct e_ext_workspace*

    // Resource for each client that has binded to manager.
    struct wl_list resources; //struct wl_resource*

    struct
    {
        struct wl_signal request_create_workspace; //const char*
        struct wl_signal destroy;
    } events;

    struct wl_list link; //e_ext_workspace_manager::groups
};

enum e_ext_workspace_state
{
    // Workspace is visible.
    E_EXT_WORKSPACE_STATE_ACTIVE = 1 << 0,
    // Workspace wants attention.
    E_EXT_WORKSPACE_STATE_URGENT = 1 << 1,
    // Workspace is not visible.
    E_EXT_WORKSPACE_STATE_HIDDEN = 1 << 2
};

struct e_ext_workspace
{
    struct e_ext_workspace_manager* manager;

    // Group this workspace is assigned to, may be NULL.
    struct e_ext_workspace_group* group;

    char* id;
    char* name;

    uint32_t state; //bitmask enum e_ext_workspace_state
    uint32_t pending_state; //bitmask enum e_ext_workspace_state

    struct wl_array coords;

    // Resource for each client that has binded to manager.
    struct wl_list resources; //struct wl_resource*

    struct
    {
        struct wl_signal request_activate;
        struct wl_signal request_deactivate;
        struct wl_signal request_assign; //struct e_ext_workspace_group*
        struct wl_signal request_remove;
        struct wl_signal destroy;
    } events;

    struct wl_list manager_link; //e_ext_workspace_manager::workspaces
    struct wl_list group_link; //e_ext_workspace_group::workspaces
};

// Creates the manager for managing workspaces & workspace groups.
// Capabilities is a bitmask of enum e_ext_workspace_manager_capability.
// Returns NULL on fail.
struct e_ext_workspace_manager* e_ext_workspace_manager_create(struct wl_display* display, uint32_t version, uint32_t capabilities);

// Create a new group of workspaces for set of outputs.
// Returns NULL on fail.
struct e_ext_workspace_group* e_ext_workspace_group_create(struct e_ext_workspace_manager* manager);

// Assign output to workspace group.
//TODO: void e_ext_workspace_group_output_enter(struct e_ext_workspace_group* group, struct wlr_output* output);

// Remove output from workspace group.
//TODO: void e_ext_workspace_group_output_leave(struct e_ext_workspace_group* group, struct wlr_output* output);

// Destroy workspace group and unassign its workspaces.
void e_ext_workspace_group_remove(struct e_ext_workspace_group* group);

// Creates a new workspace using workspace manager.
// ID is allowed to be NULL.
// Returns NULL on fail.
struct e_ext_workspace* e_ext_workspace_create(struct e_ext_workspace_manager* manager, const char* id);

// Assign workspace to group, a workspace can only ever be assigned to one group at a time.
// Group is allowed to be NULL.
void e_ext_workspace_assign_to_group(struct e_ext_workspace* workspace, struct e_ext_workspace_group* group);

// Set the name of workspace.
// Name is copied.
void e_ext_workspace_set_name(struct e_ext_workspace* workspace, const char* name);

// Set coordinates of the workspace.
void e_ext_workspace_set_coords(struct e_ext_workspace* workspace, struct wl_array* coords);

// Set whether or not workspace is active.
void e_ext_workspace_set_active(struct e_ext_workspace* workspace, bool active);

// Set whether or not workspace wants attention.
void e_ext_workspace_set_urgent(struct e_ext_workspace* workspace, bool urgent);

// Set whether or not workspace is hidden.
void e_ext_workspace_set_hidden(struct e_ext_workspace* workspace, bool hidden);

// Destroys workspace.
void e_ext_workspace_remove(struct e_ext_workspace* workspace);