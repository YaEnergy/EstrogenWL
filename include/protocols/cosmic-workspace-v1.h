#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include "protocols/transactions.h"

#define COSMIC_WORKSPACE_V1_VERSION 2

struct wlr_output;

enum e_cosmic_workspace_manager_capability
{
    E_COSMIC_WORKSPACE_GROUP_CAPABILITY_CREATE_WORKSPACE = 1 << 0,
    
    E_COSMIC_WORKSPACE_CAPABILITY_ACTIVATE = 1 << 1,
    E_COSMIC_WORKSPACE_CAPABILITY_DEACTIVATE = 1 << 2,
    E_COSMIC_WORKSPACE_CAPABILITY_REMOVE = 1 << 3,
    E_COSMIC_WORKSPACE_CAPABILITY_RENAME = 1 << 4,
    E_COSMIC_WORKSPACE_CAPABILITY_SET_TILING_STATE = 1 << 5
};

struct e_cosmic_workspace_manager
{
    struct wl_global* global;

    struct wl_event_loop* event_loop;
    struct wl_event_source* done_idle_event;

    struct wl_list groups; //struct e_cosmic_workspace_group*

    uint32_t capabilities; //bitmask of enum e_cosmic_workspace_manager_capability

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

struct e_cosmic_workspace_group
{
    struct e_cosmic_workspace_manager* manager;

    struct wl_list outputs; //struct group_output*, internal struct
    struct wl_list workspaces; //struct e_cosmic_workspace*

    struct wl_array capabilities;

    // Resource for each client that has binded to manager.
    struct wl_list resources; //struct wl_resource*

    struct
    {
        struct wl_signal request_create_workspace; //const char*
        struct wl_signal destroy;
    } events;

    struct wl_list link; //e_cosmic_workspace_manager::groups
};

enum e_cosmic_workspace_state
{
    // Workspace is visible.
    E_COSMIC_WORKSPACE_STATE_ACTIVE = 1 << 0,
    // Workspace wants attention.
    E_COSMIC_WORKSPACE_STATE_URGENT = 1 << 1,
    // Workspace is not visible.
    E_COSMIC_WORKSPACE_STATE_HIDDEN = 1 << 2
};

enum e_cosmic_workspace_tiling_state
{
    E_COSMIC_WORKSPACE_TILING_STATE_FLOATING_ONLY = 0,
    E_COSMIC_WORKSPACE_TILING_STATE_TILING_ENABLED = 1
};

struct e_cosmic_workspace_request_tiling_state_event
{
    enum e_cosmic_workspace_tiling_state tiling_state;

    // Private
    struct wl_listener destroy;
};

struct e_cosmic_workspace
{
    // Group this workspace is assigned to.
    struct e_cosmic_workspace_group* group;

    char* name;

    uint32_t state; //bitmask enum e_cosmic_workspace_state
    uint32_t pending_state; //bitmask enum e_cosmic_workspace_state

    enum e_cosmic_workspace_tiling_state tiling_state;

    struct wl_array coords;

    // Resource for each client that has binded to manager.
    struct wl_list resources; //struct wl_resource*

    struct
    {
        struct wl_signal request_activate;
        struct wl_signal request_deactivate;
        struct wl_signal request_remove;
        struct wl_signal request_rename; //const char*, since minor version 2
        struct wl_signal request_set_tiling_state; //struct e_cosmic_workspace_request_tiling_state_event*, since minor version 2
        struct wl_signal destroy;
    } events;

    struct wl_list link; //e_cosmic_workspace_group::workspaces
};

// Creates the manager for managing workspaces & workspace groups.
// Capabilities is a bitmask of enum e_cosmic_workspace_manager_capability.
// Returns NULL on fail.
struct e_cosmic_workspace_manager* e_cosmic_workspace_manager_create(struct wl_display* display, uint32_t version, uint32_t capabilities);

// Create a new group of workspaces for set of outputs.
// Returns NULL on fail.
struct e_cosmic_workspace_group* e_cosmic_workspace_group_create(struct e_cosmic_workspace_manager* manager);

// Assign output to workspace group.
void e_cosmic_workspace_group_output_enter(struct e_cosmic_workspace_group* group, struct wlr_output* output);

// Remove output from workspace group.
void e_cosmic_workspace_group_output_leave(struct e_cosmic_workspace_group* group, struct wlr_output* output);

// Destroy workspace group and its workspaces.
void e_cosmic_workspace_group_remove(struct e_cosmic_workspace_group* group);

// Creates a new workspace inside workspace group.
// Returns NULL on fail.
struct e_cosmic_workspace* e_cosmic_workspace_create(struct e_cosmic_workspace_group* group);

// Name is copied.
void e_cosmic_workspace_set_name(struct e_cosmic_workspace* workspace, const char* name);

// Set coordinates of the workspace.
void e_cosmic_workspace_set_coords(struct e_cosmic_workspace* workspace, struct wl_array* coords);

// Set whether or not workspace has tiling behaviour.
void e_cosmic_workspace_set_tiling_state(struct e_cosmic_workspace* workspace, enum e_cosmic_workspace_tiling_state tiling_state);

// Set whether or not workspace is active.
void e_cosmic_workspace_set_active(struct e_cosmic_workspace* workspace, bool active);

// Set whether or not workspace wants attention.
void e_cosmic_workspace_set_urgent(struct e_cosmic_workspace* workspace, bool urgent);

// Set whether or not workspace is hidden.
void e_cosmic_workspace_set_hidden(struct e_cosmic_workspace* workspace, bool hidden);

// Destroys workspace.
void e_cosmic_workspace_remove(struct e_cosmic_workspace* workspace);