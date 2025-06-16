#include "util/log.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "util/filesystem.h"

#define LOG_DIR_PATH ".local/share/estrogenwl"
#define LOG_FILE_PATH ".local/share/estrogenwl/estrogenwl.log"
#define LOG_PREV_FILE_PATH ".local/share/estrogenwl/estrogenwl_prev.log"

#define LOG_MSG_MAX_LENGTH 1024

FILE* logFile = NULL;

static char* get_time_string(void)
{
    time_t t;
    time(&t);

    char* timeString = ctime(&t);
    
    //replace new line character with null-terminator
    timeString[strlen(timeString) - 1] = '\0';

    return timeString;
}

//returned char* must be freed on finish
static char* get_path_in_home(const char* path)
{
    int fileNameBufferLength = FILENAME_MAX < 1024 ? FILENAME_MAX : 1024;
    
    char* homePath = getenv("HOME");

    //no home path
    if (homePath == NULL)
        return NULL;

    char* fullPath = calloc(fileNameBufferLength, sizeof(char));

    //allocation fail
    if (fullPath == NULL)
        return NULL;
    
    //join strings together, returns supposed to written length
    int length = snprintf(fullPath, sizeof(char) * fileNameBufferLength, "%s/%s", homePath, path);

    //path was too long
    if (length >= fileNameBufferLength)
    {
        free(fullPath);
        return NULL;
    }

    return fullPath;
}

//TODO: add error codes for when logFile is NULL

int e_log_init(void)
{
    if (logFile != NULL)
        return 1;
    
    char* logDirPath = get_path_in_home(LOG_DIR_PATH);

    if (logDirPath == NULL)
        return 1;
    
    if (!e_directory_exists(logDirPath))
    {
        if (e_directory_create(logDirPath) != 0)
        {
            free(logDirPath);
            return 1;
        }
    } 

    free(logDirPath);

    //open log file path for writing
    char* logFilePath = get_path_in_home(LOG_FILE_PATH);

    if (logFilePath == NULL)
        return 1;

    //rename previous log file if exists
    if (e_file_exists(logFilePath))
    {
        char* prevLogFilePath = get_path_in_home(LOG_PREV_FILE_PATH);

        if (prevLogFilePath == NULL)
            return 1;

        int renameResult = rename(logFilePath, prevLogFilePath);

        if (renameResult != 0)
            perror("failed to rename old log file\n");

        free(prevLogFilePath);
    }
    
    logFile = fopen(logFilePath, "w");

    free(logFilePath);

    if (logFile == NULL)
    {
        perror("failed to open log file\n");
        return 1;
    }

    e_log_info("ESTROGENWL LOG START");
    return 0;
}

void e_log_info(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char message[LOG_MSG_MAX_LENGTH]; //message buffer

    //print format with args into buffer, truncated if necessary
    vsnprintf(message, sizeof(char) * LOG_MSG_MAX_LENGTH, format, args);

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

    //print format with args into buffer, truncated if necessary
    vsnprintf(message, sizeof(char) * LOG_MSG_MAX_LENGTH, format, args);

    fprintf(stderr, "[ERROR (%s)] %s, errno: %s\n", get_time_string(), message, strerror(errno));

    if (logFile != NULL)
    {
        fprintf(logFile, "[ERROR (%s)] %s, errno: %s\n", get_time_string(), message, strerror(errno));
        fflush(logFile);
    }

    va_end(args);
}

void e_log_fini(void)
{
    e_log_info("Closing log...");

    if (logFile == NULL)
        return;

    fflush(logFile);
    fclose(logFile);
}
