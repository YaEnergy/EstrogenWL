#include "desktop/gamma_control_manager.h"
#include "util/log.h"

#include <stdlib.h>
#include <assert.h>

#include <wayland-util.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_gamma_control_v1.h>

#include "util/wl_macros.h"

static void e_gamma_control_manager_set_gamma(struct wl_listener* listener, void* data)
{
    struct e_gamma_control_manager* gamma_control_manager = wl_container_of(listener, gamma_control_manager, set_gamma);
    struct wlr_gamma_control_manager_v1_set_gamma_event* event = data;
    
    //control may be null
    if (event->control == NULL)
        return;
    
    struct wlr_output_state state;
    wlr_output_state_init(&state);

    //apply control to state
    wlr_gamma_control_v1_apply(event->control, &state);

    //apply new output state changes, if failed, send that it failed and destroy the control
    if (!wlr_output_commit_state(event->output, &state))
        wlr_gamma_control_v1_send_failed_and_destroy(event->control);
    
    wlr_output_state_finish(&state);
}

static void e_gamma_control_manager_destroy(struct wl_listener* listener, void* data)
{
    struct e_gamma_control_manager* gamma_control_manager = wl_container_of(listener, gamma_control_manager, destroy);

    SIGNAL_DISCONNECT(gamma_control_manager->set_gamma);
    SIGNAL_DISCONNECT(gamma_control_manager->destroy);

    free(gamma_control_manager);
}

struct e_gamma_control_manager* e_gamma_control_manager_create(struct wl_display* display)
{
    assert(display);

    struct e_gamma_control_manager* gamma_control_manager = calloc(1, sizeof(*gamma_control_manager));

    if (gamma_control_manager == NULL)
    {
        e_log_error("failed to allocate gamma_control_manager");
        return NULL;
    }

    gamma_control_manager->wlr_gamma_control_manager_v1 = wlr_gamma_control_manager_v1_create(display);

    //events

    SIGNAL_CONNECT(gamma_control_manager->wlr_gamma_control_manager_v1->events.set_gamma, gamma_control_manager->set_gamma, e_gamma_control_manager_set_gamma)
    SIGNAL_CONNECT(gamma_control_manager->wlr_gamma_control_manager_v1->events.destroy, gamma_control_manager->destroy, e_gamma_control_manager_destroy)

    return gamma_control_manager;
}