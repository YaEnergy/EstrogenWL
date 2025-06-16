#include "util/log.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include <wlr/util/log.h>

#include "util/filesystem.h"

#define LOG_DIR_PATH ".local/share/estrogenwl"
#define LOG_FILE_PATH ".local/share/estrogenwl/estrogenwl.log"
#define LOG_PREV_FILE_PATH ".local/share/estrogenwl/estrogenwl_prev.log"

#define LOG_MSG_MAX_LENGTH 1024

static const char* importance_names[] = {
    [WLR_SILENT] = "",
    [WLR_ERROR] = "ERROR",
    [WLR_INFO] = "INFO",
    [WLR_DEBUG] = "DEBUG"
};

FILE* log_file = NULL;

static char* time_string(void)
{
    time_t t;
    time(&t);

    char* t_string = ctime(&t);
    
    //replace new line character with null-terminator
    t_string[strlen(t_string) - 1] = '\0';

    return t_string;
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

static void e_log_vlog(enum wlr_log_importance importance, const char *fmt, va_list args)
{
    if (importance >= WLR_LOG_IMPORTANCE_LAST)
        return;

    char msg[LOG_MSG_MAX_LENGTH]; //message buffer

    //print format with args into buffer, truncated if necessary
    vsnprintf(msg, sizeof(char) * LOG_MSG_MAX_LENGTH, fmt, args);

    printf("[%s (%s)] %s\n", importance_names[importance], time_string(), msg);

    if (log_file != NULL)
    {
        fprintf(log_file, "[INFO (%s)] %s\n", time_string(), msg);
        fflush(log_file);
    }
}

void e_log_init(void)
{
    #if E_VERBOSE
    wlr_log_init(WLR_DEBUG, e_log_vlog);
    #else
    wlr_log_init(WLR_ERROR, e_log_vlog);
    #endif

    if (log_file != NULL)
        return;
    
    char* logDirPath = get_path_in_home(LOG_DIR_PATH);

    if (logDirPath == NULL)
        return;
    
    if (!e_directory_exists(logDirPath))
    {
        if (e_directory_create(logDirPath) != 0)
        {
            free(logDirPath);
            return;
        }
    } 

    free(logDirPath);

    //open log file path for writing
    char* logFilePath = get_path_in_home(LOG_FILE_PATH);

    if (logFilePath == NULL)
        return;

    //rename previous log file if exists
    if (e_file_exists(logFilePath))
    {
        char* prevLogFilePath = get_path_in_home(LOG_PREV_FILE_PATH);

        if (prevLogFilePath == NULL)
            return;

        int renameResult = rename(logFilePath, prevLogFilePath);

        if (renameResult != 0)
            perror("failed to rename old log file\n");

        free(prevLogFilePath);
    }
    
    log_file = fopen(logFilePath, "w");

    free(logFilePath);

    if (log_file == NULL)
    {
        perror("failed to open log file\n");
        return;
    }

    e_log_info("ESTROGENWL LOG START");
}

void e_log_info(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    e_log_vlog(WLR_INFO, fmt, args);

    va_end(args);
}

void e_log_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    e_log_vlog(WLR_ERROR, fmt, args);

    va_end(args);
}

void e_log_fini(void)
{
    e_log_info("Closing log...");

    if (log_file == NULL)
        return;

    fflush(log_file);
    fclose(log_file);
}
