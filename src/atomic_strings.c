//🫖ketl
#include "atomic_strings.h"

#include "str.h"


KETL_VECTOR_DEFINITION(ketl_atomic_strings_storage, char)
KETL_HASH_MAP_DEFINITION(ketl_atomic_strings_map, const char*, ketl_atomic_string, ketl_str_hash, ketl_str_is_equal)


void ketl_atomic_strings_init(ketl_atomic_strings* p_atomic_strings, const ketl_allocator* p_allocator) {
    ketl_atomic_strings_storage_init(&p_atomic_strings->storage, 16, p_allocator);
    ketl_atomic_strings_map_init(&p_atomic_strings->str_map, p_allocator);
}

void ketl_atomic_strings_deinit(ketl_atomic_strings* p_atomic_strings) {
    ketl_atomic_strings_map_deinit(&p_atomic_strings->str_map);
    ketl_atomic_strings_storage_deinit(&p_atomic_strings->storage);
}

ketl_atomic_string ketl_atomic_strings_get(ketl_atomic_strings* p_atomic_strings, const char* p_str, uint32_t length) {
    if (p_str == NULL) {
        return KETL_ATOMIC_STRING_EMPTY;
    }
    if (length == 0) {
        return KETL_ATOMIC_STRING_EMPTY;
    }

    char arr_buffer[256] = {'\0'};
    if (length == KETL_NULL_TERMINATED_LENGTH_32) {
        length = (uint32_t)ketl_strlen(p_str);
    } else {
        ANN_ASSERT(length < ANN_ARRAY_SIZE(arr_buffer));
        ketl_memcpy(arr_buffer, p_str, length);
        p_str = arr_buffer;
    }

    ketl_atomic_strings_storage* p_symbols = &p_atomic_strings->storage;
    ketl_atomic_strings_map* pm_symbols_map = &p_atomic_strings->str_map;
    ketl_atomic_strings_map_bucket* p_symbol_bucket = ketl_atomic_strings_map_get_or_insert_copy(pm_symbols_map, p_str, 0);
    if (p_symbol_bucket->key == p_str) {
        char* p_check_data = p_symbols->p_data;
        ketl_atomic_strings_storage_reserve(p_symbols, p_symbols->size + length + 1);
        if (p_check_data != p_symbols->p_data) {
            p_check_data = p_symbols->p_data;
            KETL_HASH_MAP_FOREACH(ketl_atomic_strings_map, pm_symbols_map, 
                __p_bucket->key = p_check_data + __p_bucket->value;
            );
        }

        const char* p_atomic_symbol = ketl_atomic_strings_storage_push_back_ref_n(p_symbols, p_str, length);
        ketl_atomic_strings_storage_push_back_copy(p_symbols, '\0');

        ANN_ASSERT(p_check_data == p_symbols->p_data);

        p_symbol_bucket->key = p_atomic_symbol;
        p_symbol_bucket->value = (ketl_atomic_string)(p_atomic_symbol - p_symbols->p_data);
    }
    // 0 (KETL_ATOMIC_STRING_EMPTY) is reserved for NULL and ""
    return p_symbol_bucket->value + 1;
}

const char* ketl_atomic_strings_get_pointer(ketl_atomic_strings* p_atomic_strings, ketl_atomic_string a_str) {
    return KETL_ATOMIC_STRING_GET_POINTER(p_atomic_strings->storage.p_data, a_str);
}
