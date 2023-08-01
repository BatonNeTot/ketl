//🍲ketl
#ifndef utils_h
#define utils_h

#include <limits.h>
#include <inttypes.h>
#include <stdbool.h>

#define KETL_STR_VALUE_IMPL(x) #x
#define KETL_STR_VALUE(x) KETL_STR_VALUE_IMPL(x)

#define KETL_FOREVER while(1)

#define KETL_FORWARD(name) \
typedef struct name name

#define KETL_DEFINE(name) KETL_FORWARD(name); struct name

#define KETL_NULL_TERMINATED_LENGTH SIZE_MAX

int64_t ketlStrToI64(const char* str, size_t length);

#endif /*utils_h*/
