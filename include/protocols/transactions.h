#pragma once

#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

// Allows a bunch of operations to be requested and done handled all at once, atomically.
//TODO: currently only for protocol implementations, but might be useful for layout tiling updates too?

// Keeps a list of transaction operations to be handled all at once when transaction is finished.
struct e_trans_session
{
    struct wl_list operations; //struct e_trans_op*
};

// An operation to be performed when transaction is finished.
struct e_trans_op
{
    uint32_t type; //operation type

    void* src;
    void* data;

    // Fired when transaction operation is destroyed. Used to destroy data.
    struct wl_signal destroy;

    struct wl_list link; //e_trans_session::operations
};

// Allows looping over operations inside current transaction of session, and destroying the current operation once finished.
#define e_trans_session_for_each_safe(pos, session) struct e_trans_op* tmp; \
wl_list_for_each_safe(pos, tmp, &session->operations, link)

// Init transaction session.
void e_trans_session_init(struct e_trans_session* session);

// Creates a new operation for current transaction in session.
// Returns NULL on fail.
struct e_trans_op* e_trans_session_add_op(struct e_trans_session* session, void* src, uint32_t type, void* data);

// Frees e_trans_op and fires the destroy signal. To destroy data, use the destroy signal.
void e_trans_op_destroy(struct e_trans_op* operation);