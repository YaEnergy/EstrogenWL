#pragma once

#include <stdbool.h>

// Filesystem functions

bool EPathIsValid(const char* path);

bool EFileExists(const char* path);

bool EDirectoryExists(const char* path);

int EDirectoryCreate(const char* path);





