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
#include "util/wl_macros.h"

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

// Focused surface was unmapped.
static void e_seat_focus_surface_unmap(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, focus_surface_unmap);

    #if E_VERBOSE
    e_log_info("focused surface was unmapped!");
    #endif

    e_seat_clear_focus(seat);

    e_cursor_set_focus_hover(seat->cursor);
}

static void e_seat_destroy(struct wl_listener* listener, void* data)
{
    struct e_seat* seat = wl_container_of(listener, seat, destroy);

    e_cursor_destroy(seat->cursor);

    wl_list_remove(&seat->keyboards);

    SIGNAL_DISCONNECT(seat->request_set_cursor);
    SIGNAL_DISCONNECT(seat->request_set_selection);
    SIGNAL_DISCONNECT(seat->destroy);

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

    // events

    SIGNAL_CONNECT(wlr_seat->events.request_set_cursor, seat->request_set_cursor, e_seat_request_set_cursor);
    SIGNAL_CONNECT(wlr_seat->events.request_set_selection, seat->request_set_selection, e_seat_request_set_selection);
    SIGNAL_CONNECT(wlr_seat->events.destroy, seat->destroy, e_seat_destroy);

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

// Set seat focus on a surface.
static void e_seat_set_focus_surface(struct e_seat* seat, struct wlr_surface* surface)
{
    assert(seat && surface);

    //already focused on surface
    if (seat->focus_surface == surface)
        return;

    if (seat->focus_surface != NULL)
        e_seat_clear_focus(seat);

    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    //set active keyboard focus to surface
    //only if there is an active keyboard and this keyboard doesn't already have focus on the surface
    if (wlr_keyboard != NULL && seat->wlr_seat->keyboard_state.focused_surface != surface)
        wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface, wlr_keyboard->keycodes, wlr_keyboard->num_keycodes, &wlr_keyboard->modifiers);
    
    seat->previous_focus_surface = seat->focus_surface;
    seat->focus_surface = surface;

    //clear focus on surface unmap
    SIGNAL_CONNECT(surface->events.unmap, seat->focus_surface_unmap, e_seat_focus_surface_unmap);
}

// Set seat focus on a view if possible.
void e_seat_set_focus_view(struct e_seat* seat, struct e_view* view)
{
    assert(seat && view);

    //Don't override focus of layer surfaces that request exclusive focus
    if (e_seat_has_exclusive_layer_focus(seat))
        return;

    #if E_VERBOSE
    e_log_info("seat focus on view");
    #endif

    if (view->surface == NULL)
    {
        e_log_error("e_seat_set_focus_view: view has no surface!");
        return;
    }

    e_seat_set_focus_surface(seat, view->surface);
    e_view_set_activated(view, true);
}

// Set seat focus on a layer surface if possible.
void e_seat_set_focus_layer_surface(struct e_seat* seat, struct wlr_layer_surface_v1* layer_surface)
{
    assert(seat && layer_surface);

    //Don't override focus of layer surfaces that request exclusive focus
    //Unless the new layer surface is on a higher layer and also requests exclusive focus
    if (e_seat_has_exclusive_layer_focus(seat) && layer_surface->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
    {
        struct wlr_layer_surface_v1* focus_layer_surface = wlr_layer_surface_v1_try_from_wlr_surface(seat->focus_surface);

        if (focus_layer_surface->current.layer >= layer_surface->current.layer)
            return;
    }

    #if E_VERBOSE
    e_log_info("seat focus on layer surface");
    #endif

    e_seat_set_focus_surface(seat, layer_surface->surface);
}

// Gets the type of surface (window or layer surface) and sets seat focus.
// This will do nothing if surface isn't of a type that should be focused on by the seat.
void e_seat_set_focus_surface_type(struct e_seat* seat, struct wlr_surface* surface)
{
    assert(seat && surface);

    struct e_desktop* desktop = seat->desktop;

    //focus on views & windows

    struct wlr_surface* root_surface = wlr_surface_get_root_surface(surface);
    struct e_view* view = e_view_from_surface(desktop, root_surface);

    if (view != NULL && !e_seat_has_focus(seat, view->surface))
    {
        e_seat_set_focus_view(seat, view);
        return;
    }

    //focus on layer surfaces that request on demand interactivity

    struct wlr_layer_surface_v1* hover_layer_surface = wlr_layer_surface_v1_try_from_wlr_surface(surface);

    //is layer surface that allows focus?
    if (hover_layer_surface != NULL && hover_layer_surface->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
    {
        if (!e_seat_has_focus(seat, hover_layer_surface->surface))
            e_seat_set_focus_layer_surface(seat, hover_layer_surface);    

        return;  
    }
}

// Returns true if seat has focus on this surface.
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

// Returns view previously in focus.
// Returns NULL if no view had focus.
struct e_view* e_seat_prev_focused_view(struct e_seat* seat)
{
    assert(seat);

    if (seat->previous_focus_surface == NULL)
        return NULL;

    return e_view_from_surface(seat->desktop, seat->previous_focus_surface);
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
    assert(seat);

    if (seat->focus_surface == NULL)
        return;

    //deactivate focused view if focused surface is one

    struct e_view* focused_view = e_seat_focused_view(seat);

    if (focused_view != NULL)
        e_view_set_activated(focused_view, false);

    //clear keyboard focus

    struct wlr_keyboard* wlr_keyboard = wlr_seat_get_keyboard(seat->wlr_seat);

    //if there is an active keyboard (and is focused), clear its focus
    if (wlr_keyboard != NULL && seat->wlr_seat->keyboard_state.focused_surface != NULL)
        wlr_seat_keyboard_notify_clear_focus(seat->wlr_seat);
    
    SIGNAL_DISCONNECT(seat->focus_surface_unmap);
    
    seat->previous_focus_surface = seat->focus_surface;
    seat->focus_surface = NULL;
}
