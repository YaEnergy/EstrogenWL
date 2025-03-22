#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "desktop/tree/container.h"

#include "util/list.h"

struct e_keyboard_config
{
    //TODO: keymap configuration

    // pressable keybinds
    struct e_list* keybinds; //struct e_keybind*

    // amount of key repeats per second
    // default: 25 hz
    int32_t repeat_rate_hz;

    // amount of milliseconds before a key starts to repeat
    // default: 600 ms
    int32_t repeat_delay_ms;
};

// server configuration
struct e_config
{
    enum e_tiling_mode current_tiling_mode;

    struct e_keyboard_config keyboard;

    // whether xwayland should start up for the first time when required or immediately
    // default: true
    bool xwayland_lazy;
};

// Inits to a default config.
void e_config_init(struct e_config* config);

void e_config_fini(struct e_config* config);

//TODO: implement e_config_parse_config_file
//bool e_config_parse_config_file(char* file_path, struct e_config* out);