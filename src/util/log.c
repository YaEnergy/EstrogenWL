#include "util/log.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include <wlr/util/log.h>

#define LOG_DIR_PATH ".local/share/estrogenwl"
#define LOG_FILE_PATH ".local/share/estrogenwl/estrogenwl.log"
#define LOG_PREV_FILE_PATH ".local/share/estrogenwl/estrogenwl_prev.log"

#define LOG_MSG_MAX_LENGTH 1024

static const char* importance_colors[] = {
    [WLR_SILENT] = "",
    [WLR_ERROR] = "\x1B[1;31m",
    [WLR_INFO] = "\x1B[1;34m",
    [WLR_DEBUG] = "\x1B[1;90m"
};

static const char* importance_names[] = {
    [WLR_SILENT] = "",
    [WLR_ERROR] = "ERROR",
    [WLR_INFO] = "INFO",
    [WLR_DEBUG] = "DEBUG"
};

static char* time_string(void)
{
    time_t t;
    time(&t);

    char* t_string = ctime(&t);
    
    //replace new line character with null-terminator
    t_string[strlen(t_string) - 1] = '\0';

    return t_string;
}

static void e_vlog(enum wlr_log_importance importance, const char* fmt, va_list args)
{
    if (importance >= WLR_LOG_IMPORTANCE_LAST || importance > wlr_log_get_verbosity())
        return;

    char msg[LOG_MSG_MAX_LENGTH]; //message buffer

    //print format with args into buffer, truncated if necessary
    vsnprintf(msg, sizeof(char) * LOG_MSG_MAX_LENGTH, fmt, args);

    printf("%s[%s (%s)] %s\n", importance_colors[importance], importance_names[importance], time_string(), msg);
}

void e_log_init(void)
{
    #if E_VERBOSE
    wlr_log_init(WLR_DEBUG, e_vlog);
    #else
    wlr_log_init(WLR_ERROR, e_vlog);
    #endif

    e_log_info("-~- ESTROGENWL LOG START -~-");
}

void e_log_info(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    e_vlog(WLR_INFO, fmt, args);

    va_end(args);
}

void e_log_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    e_vlog(WLR_ERROR, fmt, args);

    va_end(args);
}
