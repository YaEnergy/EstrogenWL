#include "desktop/gamma_control_manager.h"
#include "desktop/surface.h"

#include <stdlib.h>

#include <wayland-util.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_gamma_control_v1.h>

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

    wl_list_remove(&gamma_control_manager->set_gamma.link);
    wl_list_remove(&gamma_control_manager->destroy.link);

    free(gamma_control_manager);
}

struct e_gamma_control_manager* e_gamma_control_manager_create(struct wl_display* display)
{
    struct e_gamma_control_manager* gamma_control_manager = calloc(1, sizeof(struct e_gamma_control_manager));

    gamma_control_manager->wlr_gamma_control_manager_v1 = wlr_gamma_control_manager_v1_create(display);

    //events
    gamma_control_manager->set_gamma.notify = e_gamma_control_manager_set_gamma;
    wl_signal_add(&gamma_control_manager->wlr_gamma_control_manager_v1->events.set_gamma, &gamma_control_manager->set_gamma);

    gamma_control_manager->destroy.notify = e_gamma_control_manager_destroy;
    wl_signal_add(&gamma_control_manager->wlr_gamma_control_manager_v1->events.destroy, &gamma_control_manager->destroy);

    return gamma_control_manager;
}