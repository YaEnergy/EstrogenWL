#include "protocols/transactions.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include "util/wl_macros.h"

// Init transaction session.
void e_trans_session_init(struct e_trans_session* session)
{
    wl_list_init(&session->operations);
}

// Creates a new operation for current transaction in session.
// Returns NULL on fail.
struct e_trans_op* e_trans_session_add_op(struct e_trans_session* session, void* src, uint32_t type, void* data)
{
    if (session == NULL)
        return NULL;

    struct e_trans_op* op = calloc(1, sizeof(*op));

    if (op == NULL)
        return NULL;

    op->type = type;
    op->src = src;
    op->data = data;

    wl_signal_init(&op->destroy);

    wl_list_append(session->operations, &op->link);

    return op;
}

// Destroy all current operations inside session, basically starting a new transaction.
void e_trans_session_clear(struct e_trans_session* session)
{
    struct e_trans_op* op;
    struct e_trans_op* tmp;
    e_trans_session_for_each_safe(op, tmp, session)
    {
        e_trans_op_destroy(op);
    }
}

// Frees e_trans_op and emits the destroy signal. To destroy data, use the destroy signal.
void e_trans_op_destroy(struct e_trans_op* operation)
{
    if (operation == NULL)
        return;

    wl_signal_emit_mutable(&operation->destroy, NULL);

    wl_list_remove(&operation->link);

    free(operation);
}