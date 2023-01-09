#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#define NO_COLOR_FMT  "[*] %s - %s: "
#define COLOR_FMT     "%s[*] %s %s\x1b[m: "

#include "config.h"
#include "util.h"
#include "log.h"

#define __ignore_debug_

extern bool color_enabled;

static char *log_level_mappings[] = {
    [L_INFO]    = "I",
    [L_WARN]    = "W",
    [L_ERROR]   = "E",
    [L_DEBUG]   = "D",
};

static char *log_level_ccodes[] = {
    [L_INFO]    = "\x1b[32m",
    [L_WARN]    = "\x1b[35m",
    [L_ERROR]   = "\x1b[31m",
    [L_DEBUG]   = "\x1b[34m",
};

int o_log(log_levels_t level, char *fmt, ...) {
    va_list arguments;
    char *hr_time, *final_fmt, prefix[257];
    time_t now;

#ifdef __ignore_debug_
    if (level == L_DEBUG)
        return 0;
#endif

    va_start(arguments, fmt);
    time(&now);
    hr_time = ctime(&now);
    /* Removes \n at end */
    hr_time[strlen(hr_time) - 1] = '\0';

    if (color_enabled) 
        snprintf(prefix, 256, COLOR_FMT, log_level_ccodes[level], 
                log_level_mappings[level], hr_time);
    else
        snprintf(prefix, 256, NO_COLOR_FMT, log_level_mappings[level], hr_time);

    final_fmt = xmalloc(257);
    strcpy(final_fmt, prefix);
    final_fmt = xrealloc(final_fmt, 257 + strlen(fmt));
    strcat(final_fmt, fmt);
    strcat(final_fmt, "\n");

    vfprintf(stderr, final_fmt, arguments);

    free(final_fmt);

    return 0;
}

int o_plog(char *msg) {
    char *error_msg;

    error_msg = strerror(errno);
    return o_log(L_ERROR, "%s: %s", msg, error_msg);
}
