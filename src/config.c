#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "desktop/tree/container.h"

#include "input/keybind.h"

#include "util/list.h"

struct e_config* e_config_create()
{
    struct e_config* config = calloc(1, sizeof(*config));

    if (config == NULL)
        return NULL;

    config->current_tiling_mode = E_TILING_MODE_HORIZONTAL;

    config->keyboard.keybinds = e_list_create(10);
    config->keyboard.repeat_rate_hz = 25;
    config->keyboard.repeat_delay_ms = 600;

    config->xwayland_lazy = true;

    return config;
}

void e_config_destroy(struct e_config* config)
{
    assert(config);
    
    for (int i = 0; i < config->keyboard.keybinds->count; i++)
    {
        struct e_keybind* keybind = e_list_at(config->keyboard.keybinds, i);

        if (keybind != NULL)
            e_keybind_free(keybind);
    }

    e_list_destroy(config->keyboard.keybinds);

    free(config);
}