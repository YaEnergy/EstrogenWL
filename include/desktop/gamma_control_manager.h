#include <wayland-server-core.h>

#include <wlr/types/wlr_gamma_control_v1.h>

// Sets gamma when requested.
struct e_gamma_control_manager
{
    struct wlr_gamma_control_manager_v1* wlr_gamma_control_manager_v1;

    struct wl_listener set_gamma;

    struct wl_listener destroy;
};

// Creates a gamma control manager.
// Destroys itself when done.
// Returns NULL on fail.
struct e_gamma_control_manager* e_gamma_control_manager_create(struct wl_display* display);
