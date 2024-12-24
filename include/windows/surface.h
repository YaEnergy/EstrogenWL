#pragma once

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

//returns a wlr_surface pointer at the specified layout coords, 
//also outs the surface's node, and translates the layout coords to the surface coords
struct wlr_surface* e_wlr_surface_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_scene_node** snode, double* sx, double* sy);