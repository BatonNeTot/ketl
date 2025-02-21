//🫖ketl
#include "atomic_strings.h"

#include "str.h"


KETL_NAMED_VECTOR_DEFINITION(strings_storage, char)
KETL_NAMED_HASH_MAP_DEFINITION(strings_map, const char*, uint32_t, ketl_str_hash, ketl_str_is_equal)


void ketl_atomic_strings_init(ketl_atomic_strings* pAtomicStrings, const ketl_allocator* pAllocator) {
    strings_storage_init(&pAtomicStrings->vStorage, 16, pAllocator);
    strings_map_init(&pAtomicStrings->mStrMap, pAllocator);
}

void ketl_atomic_strings_deinit(ketl_atomic_strings* pAtomicStrings) {
    strings_map_deinit(&pAtomicStrings->mStrMap);
    strings_storage_deinit(&pAtomicStrings->vStorage);
}

atomic_string ketl_atomic_strings_get(ketl_atomic_strings* pAtomicStrings, const char* pStr, uint32_t length) {
    if (length == KETL_NULL_TERMINATED_LENGTH_32) {
        length = strlen(pStr);
    }
    strings_storage* pvSymbols = &pAtomicStrings->vStorage;
    strings_map* pmSymbolsMap = &pAtomicStrings->mStrMap;
    strings_map_bucket* pSymbolBucket = strings_map_get_or_insert_copy(pmSymbolsMap, pStr, 0);
    if (pSymbolBucket->key == pStr) {
        char* pCheckData = pvSymbols->pData;
        const char* pAtomicSymbol = strings_storage_push_back_ref_n(pvSymbols, pStr, length);
        if (pCheckData != pvSymbols->pData) {
            pCheckData = pvSymbols->pData;
            KETL_NAMED_HASH_MAP_FOREACH(strings_map, const char*, uint16_t, pmSymbolsMap, 
            __pBucket->key = pCheckData + __pBucket->value;);
        }
        pSymbolBucket->key = pAtomicSymbol;
        pSymbolBucket->value = pAtomicSymbol - pvSymbols->pData;
    }
    return pSymbolBucket->value;
}
