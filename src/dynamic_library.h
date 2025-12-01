//🫖ketl
#ifndef ketl_dynamic_library_h
#define ketl_dynamic_library_h


#include "ketl/utils.h"

typedef struct {
    const char* p_name;
    uint8_t* p_opcodes;
    uint64_t opcodes_size;
} export_info;

typedef struct {
    const char* p_filename;
    export_info* p_infos;
    uint64_t count;
} export_header;

typedef struct {
    const char* p_name;
    uint64_t index;
} import_info;

typedef struct {
    const char* p_filename;
    import_info* p_imports;
    uint64_t count;
} import_header;

typedef void(*ketl_dynamic_library_func_t)(void);

void ketl_dynamic_library_flush_function(export_header* p_export, import_header* p_import, const uint64_t import_count);

ketl_dynamic_library_func_t ketl_dynamic_library_load_function(const char* p_filename, const char* p_function_name);

#endif
