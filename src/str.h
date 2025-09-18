//🫖ketl
#ifndef ketl_str_h
#define ketl_str_h

#include "ketl/utils.h"


uint64_t ketl_str_hash(const char* p_str);

bool ketl_str_is_equal(const char* restrict p_lhs_str, const char* restrict p_rhs_str);

uint64_t ketl_str_hash_n(const char* p_str, uint32_t length);

bool ketl_str_is_equal_n(const char* restrict p_lhs_str, const char* restrict p_rhs_str, uint32_t min_length);

#endif // ketl_str_h
