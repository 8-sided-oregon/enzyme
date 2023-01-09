#ifndef _LOG_H__
#define _LOG_H__

#include <stdbool.h>

typedef enum log_levels {
    L_INFO,
    L_WARN,
    L_ERROR,
    L_DEBUG,
} log_levels_t;

int o_log(log_levels_t level, char *fmt, ...);
int o_plog(char *msg);

#endif
