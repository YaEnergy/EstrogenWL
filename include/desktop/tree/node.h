#pragma once

#include <wayland-server-core.h>

#include <wlr/types/wlr_scene.h>

struct e_view;
struct e_xdg_popup;
struct e_layer_surface;
struct e_layer_popup;
struct e_container;

// node descriptor

//type void* data of e_node_desc holds
enum e_node_desc_type
{
    E_NODE_DESC_UNKNOWN = 0, //void*
    E_NODE_DESC_VIEW = 1, //struct e_view*
    E_NODE_DESC_XDG_POPUP = 2, //struct e_xdg_popup*
    E_NODE_DESC_LAYER_SURFACE = 3, //struct e_layer_surface*
    E_NODE_DESC_LAYER_POPUP = 4, //struct e_layer_popup*
};

struct e_node_desc
{
    enum e_node_desc_type type;
    void* data;

    struct wl_listener destroy;
};

//attaches to the data of the node, and is automatically destroyed on node destruction
struct e_node_desc* e_node_desc_create(struct wlr_scene_node* node, enum e_node_desc_type type, void* data);

//may return NULL
struct e_view* e_view_try_from_e_node_desc(struct e_node_desc* node_desc);

//may return NULL
struct e_xdg_popup* e_xdg_popup_try_from_e_node_desc(struct e_node_desc* node_desc);

//may return NULL
struct e_layer_surface* e_layer_surface_try_from_e_node_desc(struct e_node_desc* node_desc);

//may return NULL
struct e_layer_popup* e_layer_popup_try_from_e_node_desc(struct e_node_desc* node_desc);
