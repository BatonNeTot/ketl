//🫖ketl
#ifndef ketl_utils_h
#define ketl_utils_h

#define ANN_INCLUDE_DEFAULT_ASSERT
#include "annex/annex.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#define KETL_NULL_TERMINATED_LENGTH_32 ((uint32_t)-1)
#define KETL_NULL_TERMINATED_LENGTH_64 ((uint64_t)-1)

#define ANN_ARRAY_SIZE(array) (sizeof(array) / sizeof(*(array)))

int64_t ketl_str_to_i64(const char* str, size_t length);

#endif // ketl_utils_h
