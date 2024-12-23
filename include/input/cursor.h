#pragma once

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

//display of cursor image and management cursor
struct e_cursor
{
    struct wlr_cursor* wlr_cursor;

    struct wlr_xcursor_manager* xcursor_manager;
};

struct e_cursor* e_cursor_create(struct wlr_output_layout* output_layout);

void e_cursor_destroy(struct e_cursor* cursor);