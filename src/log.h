// MTR https://github.com/ougi-washi/m3u8-to-rtmp

#pragma once
#include "types.h"

namespace ce
{
    static const char* LOG_LEVEL_INFO =  "INFO"; 
    static const char* LOG_LEVEL_WARN =  "WARN";
    static const char* LOG_LEVEL_ERROR = "ERROR";

    void log_message(const char *level, const char *file, const i32 line, const char *fmt, ...);
} // namespace ce

#define log_info(...) ce::log_message(ce::LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) ce::log_message(ce::LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) ce::log_message(ce::LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
