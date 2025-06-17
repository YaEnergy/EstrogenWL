#pragma once

// Init wlr log & log file.
void e_log_init(void);

void e_log_info(const char* fmt, ...);

void e_log_error(const char* fmt, ...);
