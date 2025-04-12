//🫖ketl
#include "logging.h"

#include <stdio.h>

static ketl_log_level log_level = KETL_LOG_TRACE;

uint64_t ketl_logv(ketl_log_level level, const char* message, va_list args) {
    if (level < log_level) {
        return 0;
    }
    FILE* outfile = level >= KETL_LOG_ERROR ? stderr : stdout;
    uint64_t result = vfprintf(outfile, message, args);
    // TODO check if message ends with a '\n' and print additional '\n' only if nessesary 
    result += printf("\n");
    if (level >= KETL_LOG_BREAK_LEVEL) {
        KETL_DEBUGBREAK();
    }
    return result;
}

uint64_t ketl_log(ketl_log_level level, const char* message, ...) {
    va_list args;
    va_start(args, message);
    uint64_t result = ketl_logv(level, message, args);
    va_end(args);
    return result;
}

ketl_log_level ketl_log_get_level(void) {
    return log_level;
}

void ketl_log_set_level(ketl_log_level level) {
    log_level = level;
}
