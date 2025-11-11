//🫖ketl
#ifndef ketl_dynamic_library_h
#define ketl_dynamic_library_h


#include "ketl/utils.h"

typedef void(*ketl_dynamic_library_func_t)(void);

void ketl_dynamic_library_flush_function(const char* p_filename, const char* p_function_name, uint8_t* opcodes, uint32_t size);

ketl_dynamic_library_func_t ketl_dynamic_library_load_function(const char* p_filename, const char* p_function_name);

#endif
