//🫖ketl
#ifndef ketl_str_h
#define ketl_str_h

#include "ketl/utils.h"


uint64_t ketl_str_hash(const char* pStr);

bool ketl_str_is_equal(const char* restrict pLhsStr, const char* restrict pRhsStr);

uint64_t ketl_str_hash_n(const char* pStr, uint32_t length);

bool ketl_str_is_equal_n(const char* restrict pLhsStr, const char* restrict pRhsStr, uint32_t minLength);

#endif // ketl_str_h
