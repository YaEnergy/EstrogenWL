#pragma once

// Command functions
// usually called by keybinds

#include "server.h"

// Parses & executes the command
void e_commands_parse(struct e_server* server, const char* command);