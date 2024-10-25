// MTR https://github.com/ougi-washi/m3u8-to-rtmp

#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

void ce::log_message(const char *level, const char *file, const i32 line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    time_t now = time(NULL);
    char *date_str = ctime(&now);
    date_str[strlen(date_str) - 1] = '\0'; // Remove newline

    if (strcmp(level, ce::LOG_LEVEL_INFO) == 0) {
        fprintf(stderr, "[%s | %s] ", level, date_str);
    }
    else{
        fprintf(stderr, "[%s | %s (%s:%d)] ", level, date_str, file, line);
    }
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}
