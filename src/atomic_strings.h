//🫖ketl
#ifndef ketl_containers_atomic_strings_h
#define ketl_containers_atomic_strings_h

#include "containers/vector.h"
#include "containers/hash_map.h"

#include "ketl/utils.h"

typedef uint32_t atomic_string;

KETL_NAMED_VECTOR_DECLARATION(strings_storage, char)
KETL_NAMED_HASH_MAP_DECLARATION(strings_map, const char*, atomic_string)

KETL_DEFINE(ketl_atomic_strings) {
    strings_storage vStorage;
    strings_map mStrMap;
};

void ketl_atomic_strings_init(ketl_atomic_strings* pAtomicStrings, const ketl_allocator* pAllocator);

void ketl_atomic_strings_deinit(ketl_atomic_strings* pAtomicStrings);

atomic_string ketl_atomic_strings_get(ketl_atomic_strings* pAtomicStrings, const char* pStr, uint32_t length);

#endif // ketl_containers_atomic_strings_h
