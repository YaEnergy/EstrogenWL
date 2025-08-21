#include "input/seat.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_cursor_shape_v1.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "input/keyboard.h"
#include "input/cursor.h"

#include "desktop/layer_shell.h"
#include "desktop/tree/container.h"

#include "util/log.h"
#include "util/wl_macros.h"

#include "server.h"

//2024-12-18 22:29:22 | starting to be able to do this more on my own now, I feel like I'm learning a lot :3

static void e_seat_request_set_cursor_shape(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, request_set_cursor_shape);
    struct wlr_cursor_shape_manager_v1_request_set_shape_event* event = data;
    
    //no support for other device types
    if (event->device_type != WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_POINTER)
        return;

    //any client can request, only allow focused client to actually set the surface of the cursor
    if (event->seat_client == seat->wlr_seat->pointer_state.focused_client)
        wlr_cursor_set_xcursor(seat->cursor->wlr_cursor, seat->cursor->xcursor_manager, wlr_cursor_shape_v1_name(event->shape));
}

static void e_seat_request_set_cursor(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, request_set_cursor);
    struct wlr_seat_pointer_request_set_cursor_event* event = data;

    //any client can request, only allow focused client to actually set the surface of the cursor
    if (event->seat_client == seat->wlr_seat->pointer_state.focused_client)
        wlr_cursor_set_surface(seat->cursor->wlr_cursor, event->surface, event->hotspot_x, event->hotspot_y);
}

static void e_seat_request_set_selection(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, request_set_selection);
    struct wlr_seat_request_set_selection_event* event = data;

    e_log_info("seat request set selection");

    wlr_seat_set_selection(seat->wlr_seat, event->source, event->serial);
}

static void e_seat_request_set_primary_selection(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, request_set_primary_selection);
    struct wlr_seat_request_set_primary_selection_event* event = data;
    
    e_log_info("seat request set primary selection");

    wlr_seat_set_primary_selection(seat->wlr_seat, event->source, event->serial);
}

/* drag & drop */

// Motion during drag & drop action.
static void e_seat_drag_motion(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, current_dnd.motion);

    if (seat->current_dnd.icon != NULL)
        wlr_scene_node_set_position(&seat->current_dnd.icon->node, seat->cursor->wlr_cursor->x, seat->cursor->wlr_cursor->y);
}

// Drag & drop action ended.
static void e_seat_drag_destroy(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, current_dnd.destroy);

    SIGNAL_DISCONNECT(seat->current_dnd.motion);
    SIGNAL_DISCONNECT(seat->current_dnd.destroy);
}

// Request to start a new drag & drop action.
static void e_seat_request_start_drag(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, request_start_drag);
    struct wlr_seat_request_start_drag_event* event = data;

    #if E_VERBOSE
    e_log_info("seat request start drag");
    #endif

    if (wlr_seat_validate_pointer_grab_serial(seat->wlr_seat, event->origin, event->serial))
    {
        wlr_seat_start_pointer_drag(seat->wlr_seat, event->drag, event->serial);
    }
    else 
    {
        wlr_data_source_destroy(event->drag->source);
        e_log_error("e_seat_request_start_drag: invalid pointer grab serial!");
    }
}

// Started a new drag & drop action.
static void e_seat_start_drag(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, start_drag);
    struct wlr_drag* drag = data;

    #if E_VERBOSE
    e_log_info("seat start drag");
    #endif
    
    if (drag->icon != NULL)
    {
        seat->current_dnd.icon = wlr_scene_drag_icon_create(seat->drag_icon_tree, drag->icon);
        wlr_scene_node_set_position(&seat->current_dnd.icon->node, seat->cursor->wlr_cursor->x, seat->cursor->wlr_cursor->y);
    }

    SIGNAL_CONNECT(drag->events.motion, seat->current_dnd.motion, e_seat_drag_motion);
    SIGNAL_CONNECT(drag->events.destroy, seat->current_dnd.destroy, e_seat_drag_destroy);
}

// Finalize seat drag & drop actions.
static void e_seat_fini_dnd(struct e_seat* seat)
{
    if (seat == NULL)
    {
        e_log_error("e_seat_init_dnd: seat is NULL!");
        return;
    }

    if (seat->drag_icon_tree != NULL)
    {
        wlr_scene_node_destroy(&seat->drag_icon_tree->node);
        seat->drag_icon_tree = NULL;
        seat->current_dnd.icon = NULL; //recursive
    }

    SIGNAL_DISCONNECT(seat->request_start_drag);
    SIGNAL_DISCONNECT(seat->start_drag);
}

// Init seat drag & drop actions.
static void e_seat_init_dnd(struct e_seat* seat)
{
    if (seat == NULL)
    {
        e_log_error("e_seat_init_dnd: seat is NULL!");
        return;
    }

    seat->current_dnd.icon = NULL;
    seat->drag_icon_tree = wlr_scene_tree_create(&seat->server->scene->tree);

    SIGNAL_CONNECT(seat->wlr_seat->events.request_start_drag, seat->request_start_drag, e_seat_request_start_drag);
    SIGNAL_CONNECT(seat->wlr_seat->events.start_drag, seat->start_drag, e_seat_start_drag);
}

/* end drag & drop */

static void e_seat_handle_destroy(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, destroy);

    //destroy keyboards
    struct e_keyboard* keyboard;
    struct e_keyboard* tmp;
    wl_list_for_each_safe(keyboard, tmp, &seat->keyboards, link)
    {
        e_keyboard_destroy(keyboard);
    }

    e_cursor_destroy(seat->cursor);

    e_seat_fini_dnd(seat);

    wl_list_remove(&seat->keyboards);

    SIGNAL_DISCONNECT(seat->request_set_cursor);
    SIGNAL_DISCONNECT(seat->request_set_selection);
    SIGNAL_DISCONNECT(seat->request_set_primary_selection);

    SIGNAL_DISCONNECT(seat->destroy);
    
    SIGNAL_DISCONNECT(seat->request_set_cursor_shape);

    free(seat);
}

struct e_seat* e_seat_create(struct e_server* server, struct wlr_output_layout* output_layout, const char* name)
{
    assert(server && output_layout && name);

    struct e_seat* seat = calloc(1, sizeof(*seat));

    if (seat == NULL)
    {
        e_log_error("failed to allocate seat");
        return NULL;
    }

    struct wlr_seat* wlr_seat = wlr_seat_create(server->display, name);

    seat->server = server;
    seat->wlr_seat = wlr_seat;

    seat->focus.active_view_container = NULL;
    seat->focus.focused_layer_surface = NULL;

    seat->cursor = e_cursor_create(seat, output_layout);

    e_seat_init_dnd(seat);

    seat->cursor_shape_manager = wlr_cursor_shape_manager_v1_create(server->display, 1);

    wl_list_init(&seat->keyboards);

    // events

    SIGNAL_CONNECT(wlr_seat->events.request_set_cursor, seat->request_set_cursor, e_seat_request_set_cursor);
    SIGNAL_CONNECT(wlr_seat->events.request_set_selection, seat->request_set_selection, e_seat_request_set_selection);
    SIGNAL_CONNECT(wlr_seat->events.request_set_primary_selection, seat->request_set_primary_selection, e_seat_request_set_primary_selection);
    
    SIGNAL_CONNECT(wlr_seat->events.destroy, seat->destroy, e_seat_handle_destroy);

    SIGNAL_CONNECT(seat->cursor_shape_manager->events.request_set_shape, seat->request_set_cursor_shape, e_seat_request_set_cursor_shape);

    return seat;
}

// input devices & capability

static void e_seat_update_capabilities(struct e_seat* seat)
{
    //send to clients what the capabilities of this seat are

    //seat capabilities mask, always has capability for pointers
    uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER;

    //keyboard available?
    if (!wl_list_empty(&seat->keyboards))
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;

    wlr_seat_set_capabilities(seat->wlr_seat, capabilities);
}

static void e_seat_add_keyboard(struct e_seat* seat, struct wlr_keyboard* wlr_keyboard)
{
    e_log_info("adding keyboard input device");

    struct e_keyboard* keyboard = e_keyboard_create(wlr_keyboard, seat);
    
    if (keyboard == NULL)
    {
        e_log_error("e_seat_add_keyboard: failed to add keyboard");
        return;
    }

    //add to seat
    wl_list_insert(&seat->keyboards, &keyboard->link);
    wlr_seat_set_keyboard(seat->wlr_seat, keyboard->wlr_keyboard);

    e_seat_update_capabilities(seat);
}

void e_seat_add_input_device(struct e_seat* seat, struct wlr_input_device* input)
{
    assert(seat && input);

    switch(input->type)
    {
        case WLR_INPUT_DEVICE_KEYBOARD:
            e_log_info("new keyboard input device");

            e_seat_add_keyboard(seat, wlr_keyboard_from_input_device(input));
            break;
        case WLR_INPUT_DEVICE_POINTER:
            e_log_info("new pointer input device");

            //attach pointer to cursor
            wlr_cursor_attach_input_device(seat->cursor->wlr_cursor, input);
            
            break;
        default:
            e_log_info("new unsupported input device, ignoring...");
            break;
    }
}

// focus

static bool seat_has_exclusive_focus(const struct e_seat* seat)
{
    assert(seat);

    return (seat->focus.focused_layer_surface != NULL && e_layer_surface_get_layer(seat->focus.focused_layer_surface) >= ZWLR_LAYER_SHELL_V1_LAYER_TOP && e_layer_surface_get_interactivity(seat->focus.focused_layer_surface) == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
}

static void seat_set_keyboard_focus(struct e_seat* seat, struct wlr_surface* surface)
{
    assert(seat);

    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    if (wlr_keyboard == NULL || seat->wlr_seat->keyboard_state.focused_surface == surface)
        return;

    //clear old keyboard focus if there
    if (seat->wlr_seat->keyboard_state.focused_surface != NULL)
        wlr_seat_keyboard_notify_clear_focus(seat->wlr_seat);
    
    //set active keyboard focus to surface
    //only if there is an active keyboard and this keyboard doesn't already have focus on the surface
    if (surface != NULL)
        wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface, wlr_keyboard->keycodes, wlr_keyboard->num_keycodes, &wlr_keyboard->modifiers);
}

static bool seat_set_focus_raw(struct e_seat* seat, struct wlr_surface* surface, bool replace_exclusive)
{
    assert(seat);

    //already focused on surface
    if (e_seat_has_focus(seat, surface))
        return false;

    if (seat_has_exclusive_focus(seat) && !replace_exclusive)
    {
        e_log_info("desktop has exclusive focus!: not focusing on given surface");
        return false;
    }

    seat_set_keyboard_focus(seat, surface);

    return true;
}

static void seat_set_active_view_container(struct e_seat* seat, struct e_view_container* view_container)
{
    assert(seat);

    //don't set it to the same view container
    if (seat->focus.active_view_container == view_container)
        return;

    //TODO: if there were multiple seats, another seat could still keep this view container active
    if (seat->focus.active_view_container != NULL)
        e_view_set_activated(seat->focus.active_view_container->view, false);

    seat->focus.active_view_container = view_container;

    if (view_container != NULL)
    {
        e_view_set_activated(view_container->view, true);
        e_container_raise_to_top(&view_container->base);

        struct e_output* output = (view_container->base.workspace != NULL) ? view_container->base.workspace->output : NULL;

        if (output != NULL && output->active_workspace != view_container->base.workspace)
            e_output_display_workspace(output, view_container->base.workspace);
    }
}

bool e_seat_set_focus_view_container(struct e_seat* seat, struct e_view_container* view_container)
{
    assert(seat);

    seat_set_active_view_container(seat, view_container);

    return seat_set_focus_raw(seat, (view_container != NULL) ? view_container->view->surface : NULL, false);
}

bool e_seat_set_focus_layer_surface(struct e_seat* seat, struct e_layer_surface* layer_surface)
{
    assert(seat);

    //don't set it to the same layer surface
    if (seat->focus.focused_layer_surface == layer_surface)
        return false;

    //unfocus current layer surface and attempt to set focus to active view container
    if (layer_surface == NULL)
    {
        seat->focus.focused_layer_surface = NULL;
        e_seat_set_focus_view_container(seat, seat->focus.active_view_container);
        return true;
    }

    enum zwlr_layer_shell_v1_layer layer = e_layer_surface_get_layer(layer_surface);
    enum zwlr_layer_surface_v1_keyboard_interactivity interactivity = e_layer_surface_get_interactivity(layer_surface);

    if (interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
        return false;

    bool replace_exclusive = (layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP && interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);

    //new exclusive must be on a higher layer
    if (seat_has_exclusive_focus(seat) && replace_exclusive)
        replace_exclusive = layer > e_layer_surface_get_layer(seat->focus.focused_layer_surface);

    //attempt to focus layer surface

    struct wlr_surface* surface = layer_surface->scene_layer_surface_v1->layer_surface->surface;

    if (seat_set_focus_raw(seat, surface, replace_exclusive))
    {
        seat->focus.focused_layer_surface = layer_surface;
        return true;
    }
    else 
    {
        return false;
    }
}

// Returns true if seat has focus on this surface.
bool e_seat_has_focus(struct e_seat* seat, struct wlr_surface* surface)
{
    assert(seat);

    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    return (wlr_keyboard == NULL && surface == NULL) || (wlr_keyboard != NULL && seat->wlr_seat->keyboard_state.focused_surface == surface);
}

// Destroy seat.
void e_seat_destroy(struct e_seat* seat)
{
    wlr_seat_destroy(seat->wlr_seat);
}