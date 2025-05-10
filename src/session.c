#include "session.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <sys/wait.h>

#include "util/log.h"

//TODO: document config paths used in EstrogenWL
//TODO: move get_config_path & get_relative_config_path
//FIXME: possibility of two forward slashes next to eachother in file paths, might cause issues later?

// Buffer size for config paths (env, autostart, ...)
#define CONFIG_PATHS_MAX_SIZE 1024

#define ENV_LINE_MAX_SIZE 1024

#define SHELL_PATH "/bin/sh"

enum process_env_line_status
{
    PROCESS_ENV_LINE_SUCCESS = 0,
    PROCESS_ENV_LINE_EMPTY = 1,
    PROCESS_ENV_LINE_TOO_LONG = 2,
    PROCESS_ENV_LINE_NO_NAME_VALUE_SEPARATOR = 3,
    PROCESS_ENV_LINE_NO_VALUE = 4
};

// Checks if string is only whitespace characters or given string is NULL.
// Whitespace characters: \n, \r, \t, and spaces
static bool e_str_is_empty_or_null(const char* string)
{
    if (string == NULL)
        return true;

    size_t i = 0;
    while (string[i] != '\0')
    {
        char character = string[i];

        if (character != '\n' && character != '\r' && character != '\t' && character != ' ')
            return false;

        i++;
    }

    return string[0] != '\0';
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

// Process environment file line.
static enum process_env_line_status process_env_line(const char* line)
{
    if (e_str_is_empty_or_null(line))
        return PROCESS_ENV_LINE_EMPTY;

    char line_cpy[ENV_LINE_MAX_SIZE];
    size_t length = snprintf(line_cpy, ENV_LINE_MAX_SIZE - 1, "%s", line);

    if (length >= ENV_LINE_MAX_SIZE)
        return PROCESS_ENV_LINE_TOO_LONG;

    //change line-break character to null-terminator if exists
    if (line_cpy[length - 1] == '\n')
    {
        line_cpy[length - 1] = '\0';
        length--;
    }

    //start of line is name.
    char* name = line_cpy;

    char* equal_sign = strchr(line_cpy, '=');

    if (equal_sign == NULL)
        return PROCESS_ENV_LINE_NO_NAME_VALUE_SEPARATOR;

    //use null-terminator to separate name & value into 2 strings,
    //line is no longer allowed to be used in current iteration after this
    equal_sign[0] = '\0';

    char* value = equal_sign + 1;

    if (value[0] == '\0')
        return PROCESS_ENV_LINE_NO_VALUE;

    setenv(name, value, true);

    e_log_info("env var %s set to %s", name, value);

    return PROCESS_ENV_LINE_SUCCESS;
}

// Returns if status for processing environment line was an error.
static bool process_env_line_status_is_error(enum process_env_line_status status)
{
    return status != PROCESS_ENV_LINE_SUCCESS && status != PROCESS_ENV_LINE_EMPTY;
}

// Set environment variables from pairs separated by line breaks inside file at given path.
// Format: (name)=(value)
bool e_session_init_env(void)
{
    char env_path[CONFIG_PATHS_MAX_SIZE];
    size_t env_path_length = get_relative_config_path(env_path, CONFIG_PATHS_MAX_SIZE - 1, "environment");

    //path too long
    if (env_path_length >= CONFIG_PATHS_MAX_SIZE)
    {
        e_log_error("e_session_init_env: environment file path name is longer than %i characters. (%s, %i)", CONFIG_PATHS_MAX_SIZE - 1, env_path, env_path_length);
        return false;
    }

    e_log_info("Found environment file: %s", env_path);

    FILE* file = fopen(env_path, "r");
    
    if (file == NULL)
    {
        e_log_error("e_session_init_env: failed to open file path %s!", env_path);
        return false;
    }

    char line[ENV_LINE_MAX_SIZE];
    int line_num = 0;
    enum process_env_line_status status = PROCESS_ENV_LINE_SUCCESS;

    //keep reading lines until we can't anymore
    while (fgets(line, ENV_LINE_MAX_SIZE - 1, file) != NULL)
    {
        line_num++;
        status = process_env_line(line);

        if (status == PROCESS_ENV_LINE_EMPTY)
            e_log_info("e_session_init_env: line %i is empty! Skipping...", line_num);
        else if (status == PROCESS_ENV_LINE_TOO_LONG)
            e_log_error("e_session_init_env: line %i is longer than %i characters!", line_num, ENV_LINE_MAX_SIZE - 1);
        else if (status == PROCESS_ENV_LINE_NO_NAME_VALUE_SEPARATOR)
            e_log_error("e_session_init_env: line %i has no = separator for name and value!", line_num);
        else if (status == PROCESS_ENV_LINE_NO_VALUE)
            e_log_error("e_session_init_env: line %i has no value!", line_num);

        if (process_env_line_status_is_error(status))
        {
            e_log_info("e_session_init_env: Error encountered! Stopping...");
            break;
        }
    }

    fclose(file);

    return !process_env_line_status_is_error(status);
}

// Run the autostart script. (autostart.sh in EstrogenWL's config dir)
// Returns true if fork (duplicating process) was successful, otherwise false.
bool e_session_autostart_run(void)
{
    char autostart_path[CONFIG_PATHS_MAX_SIZE];
    size_t autostart_path_length = get_relative_config_path(autostart_path, CONFIG_PATHS_MAX_SIZE - 1, "autostart.sh");

    //path too long
    if (autostart_path_length >= CONFIG_PATHS_MAX_SIZE)
    {
        e_log_error("e_session_autostart_run: autostart.sh file path name is longer than %i characters. (%s, %i)", CONFIG_PATHS_MAX_SIZE - 1, autostart_path, autostart_path_length);
        return false;
    }

    //clone process, process id is given to current process and 0 is given to new process
    int pid = fork();

    if (pid < 0)
    {
        e_log_error("e_session_autostart_run: failed to fork");
        return false;
    }

    if (pid == 0)
    {
        //new process starts here

        //according to the wikipedia page on forking _exit must be used instead of exit

        //avoid: https://en.wikipedia.org/wiki/Zombie_process
        //thanks wikipedia and labwc and sway

        setsid(); //orphan new process to avoid zombie processes
        execl(autostart_path, autostart_path, NULL);
        _exit(1); //exit process if execl failed
    }

    return true;
}