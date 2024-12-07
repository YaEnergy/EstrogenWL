#include "types/output.h"

#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

void e_output_destroy(struct wl_listener* listener, void* data)
{
    struct e_output* output = wl_container_of(listener, output, destroy);

    wl_list_remove(&output->link);
    wl_list_remove(&output->destroy.link);
    //wl_list_remove(&output->frame.link);
    free(output);
}