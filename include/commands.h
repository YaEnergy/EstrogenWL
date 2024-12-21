#pragma once

// Command functions
// usually called by keybinds

#include "server.h"

// Parses & executes the command
void e_commands_parse(struct e_server* server, const char* command);

// Runs a command in a new shell process
void e_commands_run_command_as_new_process(const char* command);

void e_commands_kill_focussed_window(struct e_server* server);

//switches focussed window between tiled and floating
void e_commands_switch_focussed_window_type(struct e_server* server);

//TODO: kill window command stuff, also other stuff