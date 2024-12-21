#pragma once

#include <stdbool.h>

// Filesystem functions

bool e_path_is_valid(const char* path);

bool e_file_exists(const char* path);

bool e_directory_exists(const char* path);

int e_directory_create(const char* path);





