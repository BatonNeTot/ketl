//🫖ketl
#include "module.h"

#include "ketl_impl.h"
#include "dynamic_library.h"
#include "compiler/assembler.h"

#include <stdio.h>


KETL_VECTOR_DEFINITION(ketl_parameters_t, ketl_named_variable_type_info_t)

KETL_VECTOR_DEFINITION(compile_function_declarations_t, compile_function_declaration_t)

void ketl_module_init(ketl_module_t* p_module, ketl_atomic_string s_name, ketl_state* p_state) {
    p_module->s_name = s_name;
    p_module->header_loaded = false;
    p_module->body_loaded = false;
    ketl_namespace_init(&p_module->namespace, s_name, &p_state->atomic_strings, &p_state->global_namespace, p_state->p_allocator);
    compile_function_declarations_t_init(&p_module->compile_function_declarations, 4, p_state->p_allocator);
}

void ketl_module_deinit(ketl_module_t* p_module) {
    compile_function_declarations_t_deinit(&p_module->compile_function_declarations);
    ketl_namespace_deinit(&p_module->namespace);
}

static void ketl_module_add_to_namespace(ketl_module_t* p_module, ketl_namespace* p_namespace) {
    ketl_variable namespace_var = {
        .pointer = &p_module->namespace,
        .type = KETL_VARIABLE_NAMESPACE,
        .p_type = NULL,
    };

    ketl_namespace_put(p_namespace, p_module->s_name, namespace_var, false);
}

bool ketl_module_preload(ketl_module_t* p_module, const char* p_module_filename, ketl_namespace* p_namespace, ketl_state* p_state, bool print_asm) {
    if (p_module->header_loaded && p_namespace != NULL) {
        ketl_module_add_to_namespace(p_module, p_namespace);

        return true;
    }

    p_module->header_loaded = true;

    FILE *p_module_file = fopen(p_module_filename, "rb");
    ANN_ASSERT(p_module_file != NULL);
    
    fseek(p_module_file, 0L, SEEK_END);
    int64_t filesize = ftell(p_module_file);
    ANN_ASSERT(filesize >= 0);

    p_module->p_source = ketl_alloc(p_state->p_allocator, (uint64_t)filesize);

    fseek(p_module_file, 0L, SEEK_SET);
    fread(p_module->p_source, sizeof(*p_module->p_source), filesize, p_module_file);
    ANN_ASSERT(ferror(p_module_file) == 0);

    fclose(p_module_file);

    uint32_t compile_function_mark = p_state->compile_function_declarations.size;
    uint32_t error_stream_mark = p_state->error_stream.size;

    ketl_lexer_init(&p_module->lexer, p_state->p_allocator);
    ketl_lexer_build_tokens(&p_module->lexer, ketl_atomic_strings_get(&p_state->atomic_strings, p_module_filename, KETL_NULL_TERMINATED_LENGTH_32), p_module->p_source, filesize);

    ketl_variable output_variable;
    p_module->opcodes_size = 0u;
    p_module->p_opcodes = ketl_state_compile_function(p_state, &p_module->lexer, p_module->lexer.tokens.size, &p_module->namespace, &p_module->opcodes_size, NULL, 0, &output_variable, print_asm);

    for (uint32_t i = compile_function_mark; i < p_state->compile_function_declarations.size; ++i) {
        compile_function_declarations_t_push_back_ref(&p_module->compile_function_declarations, &p_state->compile_function_declarations.p_data[i]);
    }

    p_state->compile_function_declarations.size = compile_function_mark;
    p_state->error_stream.size = error_stream_mark;

    if (p_module->p_opcodes == NULL) {
        return false;
    }

    if (p_namespace != NULL) {
        ketl_module_add_to_namespace(p_module, p_namespace);
    }

    return true;
}

bool ketl_module_load(ketl_module_t* p_module, ketl_state* p_state, bool print_asm) {
    uint32_t error_stream_mark = p_state->error_stream.size;

    ketl_state_postload(p_state, &p_module->lexer, &p_module->namespace, &p_module->compile_function_declarations, print_asm);
    ketl_lexer_deinit(&p_module->lexer);
    ketl_free(p_state->p_allocator, p_module->p_source);
    
    p_state->error_stream.size = error_stream_mark;

    if (p_module->p_opcodes == NULL) {
        return false;
    }

    const char* p_func_name = ".init";

    const char* p_library_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_module->s_name);

    char a_library_filename[256] = {'\0'};
    snprintf(a_library_filename, ANN_ARRAY_SIZE(a_library_filename), "%s.dll", p_library_name);
    
    {
        uint64_t export_count = 1;
        export_info a_export[256] = {
            {
                .p_name = p_func_name,
                .p_opcodes = p_module->p_opcodes,
                .opcodes_size = p_module->opcodes_size,
            },
        };

        for (uint32_t i = 0; i < p_module->compile_function_declarations.size; ++i) {
            ANN_ASSERT(export_count < ANN_ARRAY_SIZE(a_export));

            a_export[export_count++] = (export_info){
                .p_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_module->compile_function_declarations.p_data[i].s_name),
                .p_opcodes = p_module->compile_function_declarations.p_data[i].p_opcodes,
                .opcodes_size = p_module->compile_function_declarations.p_data[i].opcodes_size,
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
        for (uint32_t i = 0; i < p_module->compile_function_declarations.size; ++i) {
            ketl_parameters_t_deinit(&p_module->compile_function_declarations.p_data[i].v_parameters);
            ketl_free(p_state->p_allocator, p_module->compile_function_declarations.p_data[i].p_opcodes);
            p_module->compile_function_declarations.p_data[i].p_variable->func = ketl_dynamic_library_load_function(a_library_filename, 
                ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_module->compile_function_declarations.p_data[i].s_name));
        }
    }

    ketl_free(p_state->p_allocator, p_module->p_opcodes);
    uint64_t(*func)(void) = (uint64_t(*)(void))ketl_dynamic_library_load_function(a_library_filename, p_func_name);

    /*output_variable.uint64 = */func();

    p_module->body_loaded = true;

    return true;
}
