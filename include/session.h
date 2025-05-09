#pragma once

#include <stdbool.h>

// Set environment variables from pairs inside environment config file
// Format: (name)=(value)
bool e_session_init_env(void);

// Run the autostart script. (autostart.sh in EstrogenWL's config dir)
// Returns true if fork (duplicating process) was successful, otherwise false.
bool e_session_autostart_run(void);