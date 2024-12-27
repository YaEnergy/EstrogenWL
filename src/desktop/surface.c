#include "desktop/surface.h"

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>

struct wlr_surface* e_wlr_surface_at(struct wlr_scene_node* node, double lx, double ly, struct wlr_scene_node** snode, double* sx, double* sy)
{
    *snode = wlr_scene_node_at(node, lx, ly, sx, sy);

    if (*snode == NULL || (*snode)->type != WLR_SCENE_NODE_BUFFER)
        return NULL;

    struct wlr_scene_buffer* buffer = wlr_scene_buffer_from_node(*snode);
    struct wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(buffer);

    if (scene_surface == NULL)
        return NULL;

    return scene_surface->surface;
}