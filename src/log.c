#include "log.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "filesystem.h"

#define LOG_FILE_PATH "/.local/share/estrogencompositor.log"
#define LOG_PREV_FILE_PATH "/.local/share/estrogencompositor_prev.log"

#define LOG_MSG_MAX_LENGTH 1024

FILE* logFile = NULL;

static char* get_time_string()
{
    time_t t;
    time(&t);

    char* timeString = ctime(&t);
    
    //replace new line character with null-terminator
    timeString[strlen(timeString) - 1] = '\0';

    return timeString;
}

static char* get_file_path_in_home(const char* path)
{
    char* homePath = getenv("HOME");

    //no home path
    if (homePath == NULL)
        return NULL;

    //full path will be too long
    if (strlen(homePath) + strlen(path) > FILENAME_MAX)
        return NULL;

    char* fullPath = calloc(strlen(homePath) + strlen(path), sizeof(char));

    //allocation fail
    if (fullPath == NULL)
        return NULL;
    
    //join strings together, returns supposed to written length
    int length = snprintf(fullPath, sizeof(char) * FILENAME_MAX, "%s%s", homePath, path);

    //path was too long
    if (length > FILENAME_MAX)
    {
        free(fullPath);
        return NULL;
    }

    return fullPath;
}

//TODO: add error codes for when logFile is NULL

int e_log_init()
{
    if (logFile != NULL)
        return 1;

    //open log file path for writing
    char* logFilePath = get_file_path_in_home(LOG_FILE_PATH);

    if (logFilePath == NULL)
        return 1;

    //rename previous log file if exists
    if (e_file_exists(logFilePath))
    {
        char* prevLogFilePath = get_file_path_in_home(LOG_PREV_FILE_PATH);

        if (prevLogFilePath == NULL)
            return 1;

        rename(logFilePath, prevLogFilePath);

        free(prevLogFilePath);
    }

    logFile = fopen(logFilePath, "w");

    free(logFilePath);

    if (logFile == NULL)
    {
        perror("failed to open log file");
        return 1;
    }

    e_log_info("ESTROGENCOMPOSITOR LOG START");
    return 0;
}

void e_log_info(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char message[LOG_MSG_MAX_LENGTH]; //message buffer

    //print format with args into buffer
    int length = vsnprintf(message, sizeof(char) * LOG_MSG_MAX_LENGTH, format, args);

    if (length >= sizeof(message))
    {
        e_log_error("Log message too long");
        va_end(args);
        return;
    }

    printf("[INFO (%s)] %s\n", get_time_string(), message);

    if (logFile != NULL)
    {
        fprintf(logFile, "[INFO (%s)] %s\n", get_time_string(), message);
        fflush(logFile);
    }

    va_end(args);
}

void e_log_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char message[LOG_MSG_MAX_LENGTH]; //message buffer

    //print format with args into buffer 
    int length = vsnprintf(message, sizeof(char) * LOG_MSG_MAX_LENGTH, format, args);

    if (length >= sizeof(message))
    {
        e_log_error("Error message too long"); //this is so funny
        va_end(args);
        return;
    }

    fprintf(stderr, "[ERROR (%s)] %s, errno: %s\n", get_time_string(), message, strerror(errno));

    if (logFile != NULL)
    {
        fprintf(logFile, "[ERROR (%s)] %s, errno: %s\n", get_time_string(), message, strerror(errno));
        fflush(logFile);
    }

    va_end(args);
}

void e_log_deinit()
{
    e_log_info("Closing log...");

    if (logFile == NULL)
        return;

    fflush(logFile);
    fclose(logFile);
}