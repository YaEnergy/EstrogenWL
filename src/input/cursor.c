#include "input/cursor.h"

#include <stdlib.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

struct e_cursor* e_cursor_create(struct wlr_output_layout* output_layout)
{
    struct e_cursor* cursor = calloc(1, sizeof(struct e_cursor));

    cursor->wlr_cursor = wlr_cursor_create();

    //boundaries and movement semantics of cursor
    wlr_cursor_attach_output_layout(cursor->wlr_cursor, output_layout);

    cursor->xcursor_manager = wlr_xcursor_manager_create(NULL, 24);

    return cursor;
}

void e_cursor_destroy(struct e_cursor* cursor)
{
    wlr_xcursor_manager_destroy(cursor->xcursor_manager);
    wlr_cursor_destroy(cursor->wlr_cursor);
}