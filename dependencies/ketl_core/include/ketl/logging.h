//🍲ketl
#ifndef logging_h
#define logging_h

#include "ketl/utils.h"

#include <stdarg.h>

typedef uint8_t ketl_log_level_t;

#define KETL_LOG_TRACE  0
#define KETL_LOG_INFO   1
#define KETL_LOG_DEBUG  2
#define KETL_LOG_WARN   3
#define KETL_LOG_ERROR  4
#define KETL_LOG_FATAL  5
#define KETL_LOG_NONE   6

#define KETL_LOG_BREAK_LEVEL KETL_LOG_WARN

uint64_t ketl_logv(ketl_log_level_t level, const char* message, va_list args);

uint64_t ketl_log(ketl_log_level_t level, const char* message, ...);

ketl_log_level_t ketl_log_get_level();

void ketl_log_set_level(ketl_log_level_t level);

#define KETL_TRACE(message, ...) ketl_log(KETL_LOG_TRACE, message, __VA_ARGS__)
#define KETL_INFO(message, ...) ketl_log(KETL_LOG_INFO, message, __VA_ARGS__)
#define KETL_DEBUG(message, ...) ketl_log(KETL_LOG_DEBUG, message, __VA_ARGS__)
#define KETL_WARN(message, ...) ketl_log(KETL_LOG_WARN, message, __VA_ARGS__)
#define KETL_ERROR(message, ...) ketl_log(KETL_LOG_ERROR, message, __VA_ARGS__)
#define KETL_FATAL(message, ...) ketl_log(KETL_LOG_FATAL, message, __VA_ARGS__)

#endif /*logging_h*/