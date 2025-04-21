#pragma once

// Command functions
// usually called by keybinds

struct e_desktop;

// Parses & executes the command
void e_commands_parse(struct e_desktop* desktop, const char* command);
