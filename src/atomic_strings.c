//🫖ketl
#include "atomic_strings.h"

#include "str.h"

#include <string.h>


KETL_VECTOR_DEFINITION(ketl_atomic_strings_storage, char)
KETL_HASH_MAP_DEFINITION(ketl_atomic_strings_map, const char*, ketl_atomic_string, ketl_str_hash, ketl_str_is_equal)


void ketl_atomic_strings_init(ketl_atomic_strings* pAtomicStrings, const ketl_allocator* pAllocator) {
    ketl_atomic_strings_storage_init(&pAtomicStrings->vStorage, 16, pAllocator);
    ketl_atomic_strings_map_init(&pAtomicStrings->mStrMap, pAllocator);
}

void ketl_atomic_strings_deinit(ketl_atomic_strings* pAtomicStrings) {
    ketl_atomic_strings_map_deinit(&pAtomicStrings->mStrMap);
    ketl_atomic_strings_storage_deinit(&pAtomicStrings->vStorage);
}

ketl_atomic_string ketl_atomic_strings_get(ketl_atomic_strings* pAtomicStrings, const char* pStr, uint32_t length) {
    if (pStr == NULL) {
        return KETL_ATOMIC_STRING_EMPTY;
    }
    if (length == 0) {
        return KETL_ATOMIC_STRING_EMPTY;
    }

    char arr_buffer[256] = {'\0'};
    if (length == KETL_NULL_TERMINATED_LENGTH_32) {
        length = (uint32_t)strlen(pStr);
    } else {
        ANN_ASSERT(length < ANN_ARRAY_SIZE(arr_buffer));
        ketl_memcpy(arr_buffer, pStr, length);
        pStr = arr_buffer;
    }

    ketl_atomic_strings_storage* pvSymbols = &pAtomicStrings->vStorage;
    ketl_atomic_strings_map* pmSymbolsMap = &pAtomicStrings->mStrMap;
    ketl_atomic_strings_map_bucket* pSymbolBucket = ketl_atomic_strings_map_get_or_insert_copy(pmSymbolsMap, pStr, 0);
    if (pSymbolBucket->key == pStr) {
        char* pCheckData = pvSymbols->pData;
        ketl_atomic_strings_storage_reserve(pvSymbols, pvSymbols->size + length + 1);
        if (pCheckData != pvSymbols->pData) {
            pCheckData = pvSymbols->pData;
            KETL_HASH_MAP_FOREACH(ketl_atomic_strings_map, pmSymbolsMap, 
            __pBucket->key = pCheckData + __pBucket->value;);
        }

        const char* pAtomicSymbol = ketl_atomic_strings_storage_push_back_ref_n(pvSymbols, pStr, length);
        ketl_atomic_strings_storage_push_back_copy(pvSymbols, '\0');

        ANN_ASSERT(pCheckData == pvSymbols->pData);

        pSymbolBucket->key = pAtomicSymbol;
        pSymbolBucket->value = (ketl_atomic_string)(pAtomicSymbol - pvSymbols->pData);
    }
    // 0 (KETL_ATOMIC_STRING_EMPTY) is reserved for NULL and ""
    return pSymbolBucket->value + 1;
}

const char* ketl_atomic_strings_get_pointer(ketl_atomic_strings* pAtomicStrings, ketl_atomic_string aStr) {
    return KETL_ATOMIC_STRING_GET_POINTER(pAtomicStrings->vStorage.pData, aStr);
}
