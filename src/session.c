#include "session.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "util/log.h"

//TODO: document config paths used in EstrogenWL
//TODO: move get_config_path & get_relative_config_path
//FIXME: possibility of two forward slashes next to eachother in file paths, might cause issues later?

// Buffer size for config paths (env, autostart, ...)
#define MAX_CONFIG_PATHS_SIZE 1024

#define MAX_ENV_LINE_SIZE 1024

// Get length of string up to specified max length.
static size_t e_strnlen(const char* string, size_t maxlen)
{
    if (string == NULL)
        return 0;

    size_t length = 0;

    while (string[length] != '\0' && length < maxlen)
        length++;

    return length;
}

// Checks for empty strings (Only whitespace characters) or NULL.
// Whitespace characters: \n, \r, \t, and spaces
static bool str_is_empty_or_null(const char* string, size_t length)
{
    if (string == NULL)
        return true;

    for (size_t i = 0; i < length; i++)
    {
        char character = string[i];

        if (character != '\n' && character != '\r' && character != '\t' && character != ' ')
            return false;
    }

    return true;
}

// Outs path to EstrogenWL's config directory with a null-terminator.
// Returns length of path (as if weren't truncated).
static size_t get_config_path(char* buffer, size_t maxlen)
{
    if (buffer == NULL)
        return 0;

    const char* xdg_config_path = getenv("XDG_CONFIG_HOME");

    //no xdg config path set, use default: $HOME/.config
    if (xdg_config_path == NULL)
    {
        const char* home_path = getenv("HOME");

        if (home_path != NULL)
            return snprintf(buffer, maxlen, "%s%s", home_path, "/.config/EstrogenWL");
        else
            return 0;
    }
    else 
    {
        return snprintf(buffer, maxlen, "%s%s", xdg_config_path, "/EstrogenWL");
    }
}

// Outs full path using file path relative to config path into buffer with a null-terminator.
// Returns length of path (as if weren't truncated).
static size_t get_relative_config_path(char* buffer, size_t maxlen, const char* path)
{
    if (buffer == NULL || path == NULL)
        return 0;

    size_t length = get_config_path(buffer, maxlen);

    if (length >= maxlen)
        return length;

    length += snprintf(buffer + length, maxlen - length, "/%s", path);

    return length;
}

// Set environment variables from pairs separated by line breaks inside file at given path.
// Format: (name)=(value)
bool e_session_init_env(void)
{
    char env_path[MAX_CONFIG_PATHS_SIZE];
    size_t env_path_length = get_relative_config_path(env_path, MAX_CONFIG_PATHS_SIZE - 1, "environment");

    //path too long
    if (env_path_length >= MAX_CONFIG_PATHS_SIZE)
    {
        e_log_error("e_session_init_env: environment file path name is longer than %i characters. (%s, %i)", MAX_CONFIG_PATHS_SIZE - 1, env_path, env_path_length);
        return false;
    }

    e_log_info("Found environment file: %s", env_path);

    FILE* file = fopen(env_path, "r");
    
    if (file == NULL)
    {
        e_log_error("e_session_init_env: failed to open file path %s!", env_path);
        return false;
    }

    char line[MAX_ENV_LINE_SIZE];
    int line_num = 0;

    //keep reading lines until we can't anymore
    while (fgets(line, MAX_ENV_LINE_SIZE - 1, file) != NULL)
    {
        line_num++;

        size_t line_length = e_strnlen(line, MAX_ENV_LINE_SIZE - 1);

        //skip empty lines
        if (str_is_empty_or_null(line, line_length))
        {
            e_log_info("e_session_init_env: line %i is empty! Skipping...", line_num);
            continue;
        }

        //change \n to \0 if there is one
        if (line[line_length - 1] == '\n')
            line[line_length - 1] = '\0'; 

        //start of line is name
        char* name = line;

        char* equal_sign = strchr(line, '=');

        if (equal_sign == NULL)
        {
            e_log_error("e_session_init_env: line %i has no = separator for name and value! Stopping...", line_num);
            break;
        }

        //use null-terminator to separate name & value into 2 strings,
        //line is no longer allowed to be used in current iteration after this
        equal_sign[0] = '\0';

        char* value = equal_sign + 1;

        if (value[0] == '\0')
        {
            e_log_error("e_session_init_env: line %i has no value! Stopping...", line_num);
            break;
        }

        setenv(name, value, 1);

        e_log_info("env var %s set to %s", name, value);
    }

    fclose(file);

    return true;
}