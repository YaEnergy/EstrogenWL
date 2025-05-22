#pragma once

#include <wlr/types/wlr_scene.h>

#include "desktop/views/view.h"

//TODO: comments
//TODO: finish

// Server decoration for a view.
struct e_decoration
{
    struct
    {
        struct wlr_scene_rect* left, right, top, bottom;
    } border;
};

// Create new server decoration for a view.
// Returns NULL on fail.
struct e_decoration* e_decoration_create(struct e_view* view);

struct wlr_box e_decoration_get_box(struct e_decoration* decoration);

void e_decoration_destroy(struct e_decoration* decoration);