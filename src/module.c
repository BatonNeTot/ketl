//🫖ketl
#include "module.h"

#include "ketl_impl.h"
#include "dynamic_library.h"
#include "compiler/assembler.h"

#include <stdio.h>


void ketl_module_init(ketl_module_t* p_module, ketl_atomic_string s_name, ketl_state* p_state) {
    p_module->s_name = s_name;
    ketl_namespace_init(&p_module->namespace, s_name, &p_state->atomic_strings, &p_state->global_namespace, p_state->p_allocator);
}

static void ketl_module_add_to_namespace(ketl_module_t* p_module, ketl_namespace* p_namespace) {
    ketl_variable namespace_var = {
        .pointer = &p_module->namespace,
        .type = KETL_VARIABLE_NAMESPACE,
        .p_type = NULL,
    };

    ketl_namespace_put(p_namespace, p_module->s_name, namespace_var, false);
}

bool ketl_module_load(ketl_module_t* p_module, ketl_namespace* p_namespace, ketl_state* p_state) {
    if (!ketl_namespace_is_empty(&p_module->namespace)) {
        ketl_module_add_to_namespace(p_module, p_namespace);

        return true;
    }
    
    const char* p_module_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_module->s_name);
    uint32_t module_name_length = ketl_strlen(p_module_name);

    char a_module_filename[256] = {0};
    ketl_memcpy(a_module_filename, p_module_name, module_name_length);
    ketl_memcpy(a_module_filename + module_name_length, ".ktl", 5);

    FILE *p_module_file = fopen(a_module_filename, "rb");
    ANN_ASSERT(p_module_file != NULL);
    
    fseek(p_module_file, 0L, SEEK_END);
    int64_t filesize = ftell(p_module_file);
    ANN_ASSERT(filesize >= 0);

    char *p_source = ketl_alloc(p_state->p_allocator, (uint64_t)filesize);

    fseek(p_module_file, 0L, SEEK_SET);
    fread(p_source, sizeof(*p_source), filesize, p_module_file);
    ANN_ASSERT(ferror(p_module_file) == 0);

    fclose(p_module_file);

    uint32_t compile_function_mark = p_state->compile_function_declarations.size;
    uint32_t error_stream_mark = p_state->error_stream.size;

    ketl_variable output_variable;
    uint32_t opcodes_size = 0u;
    uint8_t* p_opcodes = ketl_state_load(p_state, &p_module->namespace, &output_variable, &opcodes_size, p_module_name, p_source, filesize);

    ketl_free(p_state->p_allocator, p_source);
    p_state->error_stream.size = error_stream_mark;

    if (p_opcodes == NULL) {
        return false;
    }

    const char* p_func_name = ".init";

    const char* p_library_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_module->s_name);
    uint32_t library_name_length = ketl_strlen(p_library_name);

    char a_library_filename[256] = {0};
    ketl_memcpy(a_library_filename, p_library_name, library_name_length);
    ketl_memcpy(a_library_filename + library_name_length, ".dll", 4);
    
    {
        uint64_t export_count = 1;
        export_info a_export[256] = {
            {
                .p_name = p_func_name,
                .p_opcodes = p_opcodes,
                .opcodes_size = opcodes_size,
            },
        };

        for (uint32_t i = compile_function_mark; i < p_state->compile_function_declarations.size; ++i) {
            ANN_ASSERT(export_count < ANN_ARRAY_SIZE(a_export));

            a_export[export_count++] = (export_info){
                .p_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_state->compile_function_declarations.p_data[i].s_name),
                .p_opcodes = p_state->compile_function_declarations.p_data[i].p_opcodes,
                .opcodes_size = p_state->compile_function_declarations.p_data[i].opcodes_size,
            };
        }

        export_header export = {
            .p_filename = a_library_filename,
            .p_infos = a_export,
            .count = export_count,
        };

        ketl_dynamic_library_flush_function(&export, NULL, 0);
    }

    {
        for (uint32_t i = compile_function_mark; i < p_state->compile_function_declarations.size; ++i) {
            ketl_parameters_t_deinit(&p_state->compile_function_declarations.p_data[i].v_parameters);
            ketl_free(p_state->p_allocator, p_state->compile_function_declarations.p_data[i].p_opcodes);
            p_state->compile_function_declarations.p_data[i].p_variable->func = ketl_dynamic_library_load_function(a_library_filename, 
                ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_state->compile_function_declarations.p_data[i].s_name));
        }
    }

    p_state->compile_function_declarations.size = compile_function_mark;

    ketl_free(p_state->p_allocator, p_opcodes);
    uint64_t(*func)(void) = (uint64_t(*)(void))ketl_dynamic_library_load_function(a_library_filename, p_func_name);

    output_variable.uint64 = func();

    ketl_module_add_to_namespace(p_module, p_namespace);
    return true;
}

void ketl_module_deinit(ketl_module_t* p_module) {
    ketl_namespace_deinit(&p_module->namespace);
}
