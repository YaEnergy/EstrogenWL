#pragma once

#include <wlr/types/wlr_keyboard.h>

#include "server.h"

struct e_keyboard
{
    struct e_server* server;
    struct wlr_keyboard* wlr_keyboard;
};