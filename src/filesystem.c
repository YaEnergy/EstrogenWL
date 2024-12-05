#include "filesystem.h"

#include <stdbool.h>
#include <stdio.h>

#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>

bool EPathIsValid(const char* path)
{
    //TODO: Implement
    return false;
}

bool EFileExists(const char* path)
{
    FILE* file = fopen(path, "r");
    
    if (file != NULL)
    {
        fclose(file);
        return true;
    }
    else 
    {
        return false;
    }
}

bool EDirectoryExists(const char* path)
{
    DIR* dir = opendir(path);

    if (dir != NULL)
    {
        closedir(dir);
        return true;
    }
    else 
    {
        return false;
    }
}

int EDirectoryCreate(const char* path)
{
    return mkdir(path, 0777);
}
