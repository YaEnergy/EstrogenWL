#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include "protocols/transactions.h"

//TODO: group output
//TODO: comments

// First support minor version 1, later 2
#define COSMIC_WORKSPACE_V1_VERSION 1

struct wlr_output;

enum e_cosmic_workspace_manager_capability
{
    E_COSMIC_WORKSPACE_GROUP_CAPABILITY_CREATE_WORKSPACE = 1 << 0,
    
    E_COSMIC_WORKSPACE_CAPABILITY_ACTIVATE = 1 << 1,
    E_COSMIC_WORKSPACE_CAPABILITY_DEACTIVATE = 1 << 2,
    E_COSMIC_WORKSPACE_CAPABILITY_REMOVE = 1 << 3
};

struct e_cosmic_workspace_manager_v1
{
    struct wl_global* global;

    struct wl_event_loop* event_loop;
    struct wl_event_source* done_idle_event;

    struct wl_list groups; //struct e_cosmic_workspace_group_v1*

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

struct e_cosmic_workspace_group_v1
{
    struct e_cosmic_workspace_manager_v1* manager;

    struct wl_list outputs; //struct e_cosmic_workspace_v1_group_output*
    struct wl_list workspaces; //struct e_cosmic_workspace_v1*

    struct wl_array capabilities;

    // Resource for each client that has binded to manager.
    struct wl_list resources; //struct wl_resource*

    struct
    {
        struct wl_signal request_create_workspace; //const char*
        struct wl_signal destroy;
    } events;

    struct wl_list link; //e_cosmic_workspace_manager_v1::groups
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

struct e_cosmic_workspace_v1
{
    // Group this workspace is assigned to.
    struct e_cosmic_workspace_group_v1* group;

    char* name;

    uint32_t state; //bitmask enum e_cosmic_workspace_state
    uint32_t pending_state; //bitmask enum e_cosmic_workspace_state

    struct wl_array capabilities;

    struct wl_array coords;

    // Resource for each client that has binded to manager.
    struct wl_list resources; //struct wl_resource*

    struct
    {
        struct wl_signal request_activate;
        struct wl_signal request_deactivate;
        struct wl_signal request_remove;
        //struct wl_signal request_rename; since minor version 2
        //struct wl_signal request_set_tiling_state; since minor version 2
        struct wl_signal destroy;
    } events;

    struct wl_list link; //e_cosmic_workspace_group_v1::workspaces
};

// Capabilities is a bitmask of enum e_cosmic_workspace_manager_capability.
// Returns NULL on fail.
struct e_cosmic_workspace_manager_v1* e_cosmic_workspace_manager_v1_create(struct wl_display* display, uint32_t version, uint32_t capabilities);

// Returns NULL on fail.
struct e_cosmic_workspace_group_v1* e_cosmic_workspace_group_v1_create(struct e_cosmic_workspace_manager_v1* manager);

// Assign output to workspace group.
void e_cosmic_workspace_group_v1_output_enter(struct e_cosmic_workspace_group_v1* group, struct wlr_output* output);

// Remove output from workspace group.
void e_cosmic_workspace_group_v1_output_leave(struct e_cosmic_workspace_group_v1* group, struct wlr_output* output);

// Destroy workspace group and its workspaces.
void e_cosmic_workspace_group_v1_remove(struct e_cosmic_workspace_group_v1* group);

// Creates a new workspace inside workspace group.
// Returns NULL on fail.
struct e_cosmic_workspace_v1* e_cosmic_workspace_v1_create(struct e_cosmic_workspace_group_v1* group);

// Name is copied.
void e_cosmic_workspace_v1_set_name(struct e_cosmic_workspace_v1* workspace, const char* name);

void e_cosmic_workspace_v1_set_coords(struct e_cosmic_workspace_v1* workspace, struct wl_array* coords);

// Set whether or not workspace is active.
void e_cosmic_workspace_v1_set_active(struct e_cosmic_workspace_v1* workspace, bool active);

// Set whether or not workspace wants attention.
void e_cosmic_workspace_v1_set_urgent(struct e_cosmic_workspace_v1* workspace, bool urgent);

// Set whether or not workspace is hidden.
void e_cosmic_workspace_v1_set_hidden(struct e_cosmic_workspace_v1* workspace, bool hidden);

// Destroys workspace.
void e_cosmic_workspace_v1_remove(struct e_cosmic_workspace_v1* workspace);