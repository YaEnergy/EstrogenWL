#include "config.h"

#include <stdbool.h>
#include <assert.h>

#include "desktop/tree/container.h"

#include "input/keybind.h"

#include "util/list.h"

void e_config_init(struct e_config* config)
{
    assert(config);

    config->current_tiling_mode = E_TILING_MODE_HORIZONTAL;

    e_list_init(&config->keyboard.keybinds, 10);
    config->keyboard.repeat_rate_hz = 25;
    config->keyboard.repeat_delay_ms = 600;

    config->xwayland_lazy = true;
}

void e_config_fini(struct e_config* config)
{
    assert(config);
    
    for (int i = 0; i < config->keyboard.keybinds.count; i++)
    {
        struct e_keybind* keybind = e_list_at(&config->keyboard.keybinds, i);

        if (keybind != NULL)
            e_keybind_free(keybind);
    }

    e_list_fini(&config->keyboard.keybinds);
}
