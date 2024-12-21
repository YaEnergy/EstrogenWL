#include "util/filesystem.h"

#include <stdbool.h>
#include <stdio.h>

#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>

bool e_path_is_valid(const char* path)
{
    //TODO: Implement
    return false;
}

bool e_file_exists(const char* path)
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

bool e_directory_exists(const char* path)
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

int e_directory_create(const char* path)
{
    return mkdir(path, 0777);
}
