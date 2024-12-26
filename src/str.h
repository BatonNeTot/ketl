//🫖ketl
#ifndef ketl_str_h
#define ketl_str_h

#include "ketl/utils.h"


uint64_t ketl_str_hash(const char* str);

bool ketl_str_is_equal(const char* restrict lhsStr, const char* restrict rhsStr);

uint64_t ketl_str_hash_n(const char* str, uint32_t length);

bool ketl_str_is_equal_n(const char* restrict lhsStr, const char* restrict rhsStr, uint32_t minLength);

#endif // ketl_str_h