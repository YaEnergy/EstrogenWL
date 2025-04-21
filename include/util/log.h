#pragma once

int e_log_init(void);

void e_log_info(const char* format, ...);

void e_log_error(const char* format, ...);

void e_log_deinit(void);
