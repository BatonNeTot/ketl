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

static void ketl_module_add_to_namespace(ketl_module_t* p_module, ketl_namespace* p_namespace, ketl_namespace_node_info info, ketl_atomic_strings* p_atomic_strings) {
    ketl_variable namespace_var = {
        .p_pointer = &p_module->namespace,
        .kind = KETL_VARIABLE_NAMESPACE,
        .p_type = NULL,
    };

    ketl_namespace_put(p_namespace, p_module->s_name, namespace_var, info, p_atomic_strings, false);
}

bool ketl_module_preload(ketl_module_t* p_module, const char* p_module_filename, const char* p_folder_path, ketl_namespace* p_namespace, bool export, ketl_state* p_state) {
    if (p_module->header_loaded && p_namespace != NULL) {
        ketl_module_add_to_namespace(p_module, p_namespace, (ketl_namespace_node_info){ .export = export, }, &p_state->atomic_strings);

        return true;
    }

    p_module->header_loaded = true;
    ketl_module_t* p_stashed_module = p_state->p_active_module;
    p_state->p_active_module = p_module; 

    char a_fullpath[256] = {'\0'};
    uint32_t fullpath_printed = 0;
    if (p_stashed_module != NULL) { 
        fullpath_printed += snprintf(a_fullpath + fullpath_printed, ANN_ARRAY_SIZE(a_fullpath) - fullpath_printed, "%s", p_stashed_module->p_path);
    }

    if (p_folder_path != NULL && p_folder_path[0] != '\0') {
        fullpath_printed += snprintf(a_fullpath + fullpath_printed, ANN_ARRAY_SIZE(a_fullpath) - fullpath_printed, "%s", p_folder_path);
    }

    {
        p_module->p_path = ketl_alloc(p_state->p_allocator, fullpath_printed + 1);
        ketl_memcpy(p_module->p_path, a_fullpath, fullpath_printed);
        p_module->p_path[fullpath_printed] = '\0';
    }

    char a_fullpath_filename[256] = {'\0'};
    snprintf(a_fullpath_filename, ANN_ARRAY_SIZE(a_fullpath_filename), "%s%s", p_module->p_path, p_module_filename);

    FILE *p_module_file = fopen(a_fullpath_filename, "rb");
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
    p_module->p_opcodes = ketl_state_compile_function(p_state, &p_module->lexer, p_module->lexer.tokens.size, &p_module->namespace, &p_module->opcodes_size, 
        NULL, 0, ketl_state_get_none_type(p_state), 0, true, &output_variable);

    for (uint32_t i = compile_function_mark; i < p_state->compile_function_declarations.size; ++i) {
        compile_function_declarations_t_push_back_ref(&p_module->compile_function_declarations, &p_state->compile_function_declarations.p_data[i]);
    }

    p_state->compile_function_declarations.size = compile_function_mark;
    p_state->error_stream.size = error_stream_mark;

    p_state->p_active_module = p_stashed_module;

    if (p_module->p_opcodes == NULL) {
        return false;
    }

    if (p_namespace != NULL) {
        ketl_module_add_to_namespace(p_module, p_namespace, (ketl_namespace_node_info){ .export = export, }, &p_state->atomic_strings);
    }

    return true;
}

bool ketl_module_load(ketl_module_t* p_module, ketl_state* p_state) {
    uint32_t error_stream_mark = p_state->error_stream.size;

    ketl_state_postload(p_state, &p_module->lexer, &p_module->namespace, &p_module->compile_function_declarations);
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

            compile_function_declaration_t* p_compile_function_declaration = &p_module->compile_function_declarations.p_data[i];
            ketl_namespace_node* p_node = ketl_namespace_find_by_index(
                p_compile_function_declaration->p_namespace, p_compile_function_declaration->namespace_node_index);
            a_export[export_count++] = (export_info){
                .p_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_node->s_name),
                .p_opcodes = p_compile_function_declaration->p_opcodes,
                .opcodes_size = p_compile_function_declaration->opcodes_size,
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
            compile_function_declaration_t* p_compile_function_declaration = &p_module->compile_function_declarations.p_data[i];
            ketl_parameters_t_deinit(&p_compile_function_declaration->v_parameters);
            ketl_free(p_state->p_allocator, p_compile_function_declaration->p_opcodes);
            ketl_namespace_node* p_node = ketl_namespace_find_by_index(
                p_compile_function_declaration->p_namespace, p_compile_function_declaration->namespace_node_index);
            p_node->variable.cfunc = ketl_dynamic_library_load_function(a_library_filename, 
                ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_node->s_name));
        }
    }

    ketl_free(p_state->p_allocator, p_module->p_opcodes);
    uint64_t(*func)(void) = (uint64_t(*)(void))ketl_dynamic_library_load_function(a_library_filename, p_func_name);

    /*output_variable.uint64 = */func();

    p_module->body_loaded = true;

    return true;
}
