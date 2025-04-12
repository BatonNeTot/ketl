//🫖ketl
#ifndef ketl_atomic_strings_h
#define ketl_atomic_strings_h

#include "containers/vector.h"
#include "containers/hash_map.h"

#include "ketl/utils.h"

typedef uint32_t ketl_atomic_string;

#define KETL_ATOMIC_STRING_EMPTY 0

KETL_VECTOR_DECLARATION(strings_storage, char)
KETL_HASH_MAP_DECLARATION(strings_map, const char*, ketl_atomic_string)

KETL_DEFINE(ketl_atomic_strings) {
    strings_storage vStorage;
    strings_map mStrMap;
};

void ketl_atomic_strings_init(ketl_atomic_strings* pAtomicStrings, const ketl_allocator* pAllocator);

void ketl_atomic_strings_deinit(ketl_atomic_strings* pAtomicStrings);

ketl_atomic_string ketl_atomic_strings_get(ketl_atomic_strings* pAtomicStrings, const char* pStr, uint32_t length);

#endif // ketl_atomic_strings_h
