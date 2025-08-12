#pragma once

// Command functions
// usually called by keybinds

struct e_server;

// Parses & executes the command
void e_commands_parse(struct e_server* server, const char* command);
