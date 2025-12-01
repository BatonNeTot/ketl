//🫖ketl
#ifndef ketl_atomic_strings_h
#define ketl_atomic_strings_h

#include "containers/vector.h"
#include "containers/hash_map.h"

#include "ketl/utils.h"

typedef uint32_t ketl_atomic_string;

#define KETL_ATOMIC_STRING_EMPTY 0

KETL_VECTOR_DECLARATION(ketl_atomic_strings_storage, char)
KETL_HASH_MAP_DECLARATION(ketl_atomic_strings_map, const char*, ketl_atomic_string)

ANN_DEFINE(ketl_atomic_strings) {
    ketl_atomic_strings_storage storage;
    ketl_atomic_strings_map str_map;
};

void ketl_atomic_strings_init(ketl_atomic_strings* p_atomic_strings, const ketl_allocator* p_allocator);

void ketl_atomic_strings_deinit(ketl_atomic_strings* p_atomic_strings);

ketl_atomic_string ketl_atomic_strings_get(ketl_atomic_strings* p_atomic_strings, const char* p_str, uint32_t length);

const char* ketl_atomic_strings_get_pointer(ketl_atomic_strings* p_atomic_strings, ketl_atomic_string a_str);

#define KETL_ATOMIC_STRING_GET_POINTER(p_buffer, a_str) ((a_str) == 0 ? KETL_ATOMIC_STRING_EMPTY : (p_buffer) + (a_str) - 1)

#endif // ketl_atomic_strings_h
