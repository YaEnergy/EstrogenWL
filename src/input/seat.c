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
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_layout.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "desktop/views/view.h"

#include "input/keyboard.h"
#include "input/cursor.h"

#include "util/log.h"

//2024-12-18 22:29:22 | starting to be able to do this more on my own now, I feel like I'm learning a lot :3

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
    
    wlr_seat_set_selection(seat->wlr_seat, event->source, event->serial);
}

static void e_seat_destroy(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, destroy);

    e_cursor_destroy(seat->cursor);

    wl_list_remove(&seat->keyboards);

    wl_list_remove(&seat->request_set_cursor.link);
    wl_list_remove(&seat->request_set_selection.link);
    wl_list_remove(&seat->destroy.link);

    free(seat);
}

struct e_seat* e_seat_create(struct wl_display* display, struct e_desktop* desktop, struct wlr_output_layout* output_layout, const char* name)
{
    assert(display && desktop && output_layout && name);

    struct e_seat* seat = calloc(1, sizeof(*seat));

    if (seat == NULL)
    {
        e_log_error("failed to allocate seat");
        return NULL;
    }

    seat->desktop = desktop;

    struct wlr_seat* wlr_seat = wlr_seat_create(display, name);

    seat->wlr_seat = wlr_seat;
    seat->focus_surface = NULL;
    seat->previous_focus_surface = NULL;

    seat->cursor = e_cursor_create(seat, output_layout);

    wl_list_init(&seat->keyboards);

    //events
    seat->request_set_cursor.notify = e_seat_request_set_cursor;
    wl_signal_add(&wlr_seat->events.request_set_cursor, &seat->request_set_cursor);

    seat->request_set_selection.notify = e_seat_request_set_selection;
    wl_signal_add(&wlr_seat->events.request_set_selection, &seat->request_set_selection);

    seat->destroy.notify = e_seat_destroy;
    wl_signal_add(&wlr_seat->events.destroy, &seat->destroy);

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

static void e_seat_add_keyboard(struct e_seat* seat, struct wlr_input_device* input)
{
    e_log_info("adding keyboard input device");

    struct e_keyboard* keyboard = e_keyboard_create(input, seat);

    //add to seat
    wl_list_insert(&seat->keyboards, &keyboard->link);
    wlr_seat_set_keyboard(seat->wlr_seat, keyboard->wlr_keyboard);
    
    if (seat->focus_surface != NULL)
        e_seat_set_focus(seat, seat->focus_surface, true);

    e_seat_update_capabilities(seat);
}

void e_seat_add_input_device(struct e_seat* seat, struct wlr_input_device* input)
{
    assert(seat && input);

    switch(input->type)
    {
        case WLR_INPUT_DEVICE_KEYBOARD:
            e_log_info("new keyboard input device");

            e_seat_add_keyboard(seat, input);
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

void e_seat_set_focus(struct e_seat* seat, struct wlr_surface* surface, bool override_exclusive)
{
    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    //Don't override focus of layer surfaces that request exclusive focus, unless requested
    if (!override_exclusive && e_seat_has_exclusive_layer_focus(seat))
        return;

    //set active keyboard focus to surface
    //only if there is an active keyboard and this keyboard doesn't already have focus on the surface
    if (wlr_keyboard != NULL && seat->wlr_seat->keyboard_state.focused_surface != surface)
    {
        //leave currently focused surface
        if (seat->wlr_seat->keyboard_state.focused_surface != NULL)
            wlr_seat_keyboard_notify_clear_focus(seat->wlr_seat);

        wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface, wlr_keyboard->keycodes, wlr_keyboard->num_keycodes, &wlr_keyboard->modifiers);
    }
    
    seat->previous_focus_surface = seat->focus_surface;
    seat->focus_surface = surface;

    struct e_view* view = e_view_from_surface(seat->desktop, surface);
    
    if (view != NULL && view->tree != NULL)
        wlr_scene_node_raise_to_top(&view->tree->node);

    e_log_info("seat focus");
}

//returns true if seat has focus on this surface
bool e_seat_has_focus(struct e_seat* seat, struct wlr_surface* surface)
{
    if (seat->focus_surface != surface)
        return false;

    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    return (wlr_keyboard != NULL && seat->wlr_seat->keyboard_state.focused_surface == surface);
}

// Returns view currently in focus.
// Returns NULL if no view has focus.
struct e_view* e_seat_focused_view(struct e_seat* seat)
{
    assert(seat);

    if (seat->focus_surface == NULL)
        return NULL;

    return e_view_from_surface(seat->desktop, seat->focus_surface);
}

// Returns window currently in focus.
// Returns NULL if no window has focus.
struct e_window* e_seat_focused_window(struct e_seat* seat)
{
    assert(seat);

    if (seat->focus_surface == NULL)
        return NULL;

    struct e_view* view = e_seat_focused_view(seat);

    if (view == NULL)
        return NULL;
    else
        return view->container;
}

// Returns view previously in focus.
// Returns NULL if no view had focus.
struct e_view* e_seat_prev_focused_view(struct e_seat* seat)
{
    assert(seat);

    if (seat->previous_focus_surface == NULL)
        return NULL;

    return e_view_from_surface(seat->desktop, seat->previous_focus_surface);
}

// Returns window previously in focus.
// Returns NULL if no window had focus.
struct e_window* e_seat_prev_focused_window(struct e_seat* seat)
{
    assert(seat);

    if (seat->previous_focus_surface == NULL)
        return NULL;

    struct e_view* view = e_seat_prev_focused_view(seat);

    if (view == NULL)
        return NULL;
    else
        return view->container;
}

bool e_seat_has_exclusive_layer_focus(struct e_seat* seat)
{
    if (seat->focus_surface == NULL)
        return false;

    struct wlr_layer_surface_v1* layer_surface_v1 = wlr_layer_surface_v1_try_from_wlr_surface(seat->focus_surface);

    return (layer_surface_v1 != NULL && layer_surface_v1->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
}

void e_seat_clear_focus(struct e_seat* seat)
{
    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    //if there is an active keyboard, clear its focus
    if (wlr_keyboard != NULL)
        wlr_seat_keyboard_notify_clear_focus(seat->wlr_seat);
    
    seat->previous_focus_surface = seat->focus_surface;
    seat->focus_surface = NULL;
}