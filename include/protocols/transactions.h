#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

// Allows a bunch of operations to be requested and done handled all at once, atomically.
//TODO: currently only for protocol implementations, but might be useful for layout tiling updates too?

//TODO: comments

struct e_trans_session
{
    struct wl_list operations; //struct e_trans_op*
};

struct e_trans_op
{
    uint32_t type; //bitmask of request type

    void* src;
    void* data;

    // Signal to destroy data.
    struct wl_signal destroy;

    struct wl_list link; //e_trans_manager::operations
};

void e_trans_session_init(struct e_trans_session* session);

struct e_trans_op* e_trans_session_add_op(struct e_trans_session* session, void* src, uint32_t type, void* data);

void e_trans_op_destroy(struct e_trans_op* operation);