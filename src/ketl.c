//🫖ketl
#include "ketl_impl.h"

#include "compiler/parser.h"
#include "compiler/assembler.h"
#include "compiler/assembler_builder.h"

#include "executable_memory.h"
#include "dynamic_library.h"
#include "value_impl.h"
#include "type_impl.h"
#include "memory_impl.h"

#include <stdio.h>

#define FUNC_SIGNATURE_HASH(key) func_signature_hash(&(key))

static uint64_t func_signature_hash(const ketl_function_parameters* p_parameters) {
    uint64_t hash = 0u;
    uint16_t parameters_count = p_parameters->parameters_count;
    for (uint16_t i = 0u; i < parameters_count; ++i) {
        hash = ((uint64_t)p_parameters->p_parameters[i].p_type) ^ (hash << 1);
    }
    return hash;
}

#define IS_FUNC_SIGNATURES_EQUAL(lhs_key, rhs_key) is_func_signatures_equal(&(lhs_key), &(rhs_key))

static bool is_func_signatures_equal(const ketl_function_parameters* p_lhs_parameters, const ketl_function_parameters* p_rhs_parameters) {
    if (p_lhs_parameters->parameters_count != p_rhs_parameters->parameters_count) {
        return false;
    }
    uint16_t parameters_count = p_lhs_parameters->parameters_count;
    for (uint16_t i = 0u; i < parameters_count; ++i) {
        if (p_lhs_parameters->p_parameters[i].p_type != p_rhs_parameters->p_parameters[i].p_type) {
            return false;
        }
    }
    return true;
}

#define FUNC_PARAMETERS_HASH(key) func_parameters_hash(&(key))

static uint64_t func_parameters_hash(const ketl_function_parameters* p_parameters) {
    uint64_t hash = 0u;
    uint16_t parameters_count = p_parameters->parameters_count;
    // first is return type, ignore it for parameters
    for (uint16_t i = 1u; i < parameters_count; ++i) {
        hash = ((uint64_t)p_parameters->p_parameters[i].p_type) ^ (hash << 1);
    }
    return hash;
}

#define IS_FUNC_PARAMETERS_EQUAL(lhs_key, rhs_key) is_func_parameters_equal(&(lhs_key), &(rhs_key))

static bool is_func_parameters_equal(const ketl_function_parameters* p_lhs_parameters, const ketl_function_parameters* p_rhs_parameters) {
    if (p_lhs_parameters->parameters_count != p_rhs_parameters->parameters_count) {
        return false;
    }
    uint16_t parameters_count = p_lhs_parameters->parameters_count;
    // first is return type, ignore it for parameters
    for (uint16_t i = 1u; i < parameters_count; ++i) {
        if (p_lhs_parameters->p_parameters[i].p_type != p_rhs_parameters->p_parameters[i].p_type) {
            return false;
        }
    }
    return true;
}

KETL_HASH_MAP_DEFINITION(function_types_map, ketl_function_parameters, function_type_composite, FUNC_SIGNATURE_HASH, IS_FUNC_SIGNATURES_EQUAL)
KETL_HASH_MAP_DEFINITION(array_types_map_t, ketl_type*, ketl_type*, ANN_HASH, ANN_EQUAL)

KETL_VECTOR_DEFINITION(types, ketl_type*)
KETL_VECTOR_DEFINITION(string_builder_t, char)

KETL_HASH_MAP_DEFINITION(ketl_modules_t, ketl_atomic_string, ketl_module_t, ANN_HASH, ANN_EQUAL)

KETL_HASH_MAP_DEFINITION(operator_overloading_map, ketl_function_parameters, ketl_hir_tag_t, FUNC_PARAMETERS_HASH, IS_FUNC_PARAMETERS_EQUAL)

static const function_type_composite* get_function_type_composite(ketl_state* p_state, const ketl_function_parameters* p_parameters) {
    uint16_t parameters_count = p_parameters->parameters_count;
    function_types_map_bucket* p_bucket = function_types_map_get_or_insert_copy(&p_state->function_types, *p_parameters, (function_type_composite){NULL, NULL, NULL});
    if (p_bucket->value.p_signature == NULL) {
        uint64_t signature_size = sizeof(ketl_type_signature) + parameters_count * sizeof(ketl_variable_type_info_t);
        uint64_t functions_offset = ANN_ALIGN_FORWARD(signature_size, _Alignof(ketl_type_function));
        uint64_t total_alloc_size = functions_offset + 2 * sizeof(ketl_type_function);
        void* p_alloc_mem = ketl_alloc(p_state->p_allocator, total_alloc_size);

        ketl_type_signature* p_signature = p_alloc_mem;
        *p_signature = (ketl_type_signature){
            .parameters_count = parameters_count
        };
        ketl_memcpy(p_signature->a_parameters, p_parameters->p_parameters, parameters_count * sizeof(ketl_variable_type_info_t));

        p_bucket->key.p_parameters = p_signature->a_parameters;
        p_bucket->value.p_signature = p_signature;

        ketl_type_function* p_func_types = (ketl_type_function*)((char*)p_alloc_mem + functions_offset);
        uint8_t align_enum = ketl_align_find(_Alignof(void(*)(void)));
        *p_func_types = (ketl_type_function){
            .kind = KETL_TYPE_FUNCTION,
            .align_enum = align_enum,
            .size = sizeof(void(*)(void)),
            .p_type_signature = p_signature
        };
        *(p_func_types + 1) = (ketl_type_function){
            .kind = KETL_TYPE_CFUNCTION,
            .align_enum = align_enum,
            .size = sizeof(void(*)(void)),
            .p_type_signature = p_signature
        };
        p_bucket->value.p_func_type = p_func_types;
        p_bucket->value.p_cfunc_type = p_func_types + 1;
    }
    return &p_bucket->value;
}

#define LITERAL_STRING_PAIR(str)  str, (sizeof(str) - 1)

ketl_state* ketl_state_create(const ketl_allocator* p_allocator) {
    ketl_state* p_state = ketl_alloc(p_allocator, sizeof(ketl_state));
    *p_state = (ketl_state){
        .p_allocator = p_allocator
    };

    ketl_gc_init(&p_state->gc, p_allocator);
    string_builder_t_init(&p_state->error_stream, 16, p_state->p_allocator);
    ketl_atomic_strings_init(&p_state->atomic_strings, p_allocator);
    ketl_executable_memory_init(&p_state->executable_memory, p_allocator);
    ketl_namespace_init(&p_state->global_namespace, KETL_ATOMIC_STRING_EMPTY, &p_state->atomic_strings, NULL, p_allocator);
    ketl_modules_t_init(&p_state->modules, p_allocator);

    function_types_map_init(&p_state->function_types, p_allocator);
    array_types_map_t_init(&p_state->array_types, p_allocator);

    for (uint32_t i = 0; i < ANN_ARRAY_SIZE(p_state->am_hiroperator_overloading); ++i) {
        operator_overloading_map_init(p_state->am_hiroperator_overloading + i, p_allocator);
    }

    compile_function_declarations_t_init(&p_state->compile_function_declarations, 16, p_allocator);
    p_state->loading_modules = false;

#define INIT_TYPE(_var, _type) *(_type*)(_var) = (_type)
#define CREATE_PRIMITIVE_TYPE(_var_name, _name, _size, _is_integer, _is_signed)\
ketl_type* _var_name = ketl_alloc(p_allocator, sizeof(ketl_type_primitive)); \
do {\
ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR(_name));\
INIT_TYPE(_var_name, ketl_type_primitive) {\
        .s_name = s_name,\
        .kind = KETL_TYPE_PRIMITIVE,\
        .align_enum = ketl_align_find(_size),\
        .size = _size,\
        .is_integer = _is_integer,\
        .is_signed = _is_signed,\
        };\
ketl_variable namespace_variable = {\
    .kind = KETL_VARIABLE_TYPE,\
    .p_type = NULL,\
    .p_pointer = _var_name,\
};\
ketl_namespace_put(&p_state->global_namespace, s_name, namespace_variable, false);\
} while(0)

    CREATE_PRIMITIVE_TYPE(p_none,  "none", 0, false, false);
    CREATE_PRIMITIVE_TYPE(p_bool,  "bool", 1, false, false);
    CREATE_PRIMITIVE_TYPE(p_char,  "char", 1, true,  false);
    CREATE_PRIMITIVE_TYPE(p_raw,   "raw",  8, false, false);

    CREATE_PRIMITIVE_TYPE(p_i8,  "i8",   1, true,  true);
    CREATE_PRIMITIVE_TYPE(p_i16, "i16",  2, true,  true);
    CREATE_PRIMITIVE_TYPE(p_i32, "i32",  4, true,  true);
    CREATE_PRIMITIVE_TYPE(p_i64, "i64",  8, true,  true);

#define REGISTER_BINARY_OPERATOR(_hir_tag_op, _arg_type, _return_type, _hir_type)\
do {\
    ketl_variable_type_info_t parameters_array[] = { {.p_type = _return_type}, {.p_type = _arg_type}, {.p_type = _arg_type} };\
    ketl_function_parameters parameters = {\
        .p_parameters = parameters_array,\
        .parameters_count = sizeof(parameters_array) / sizeof(*parameters_array)\
    };\
\
    const function_type_composite* p_func_type_composite = get_function_type_composite(p_state, &parameters);\
    parameters.p_parameters = p_func_type_composite->p_signature->a_parameters;\
\
    operator_overloading_map_get_or_insert_copy(p_state->am_hiroperator_overloading + \
        ((_hir_tag_op - KETL_HIR_FIRST_BI_OPERATOR) >> KETL_HIR_TYPE_INSTR_SHIFT), parameters, _hir_tag_op | _hir_type);\
} while (false)

#define REGISTER_PRIMITIVE_BINARY_OPERATORS(_arg_type, _hir_type)\
do {\
    REGISTER_BINARY_OPERATOR(KETL_HIR_PLUS,             _arg_type, _arg_type, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_MINUS,            _arg_type, _arg_type, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_MULTY,            _arg_type, _arg_type, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_DIV,              _arg_type, _arg_type, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_MOD,              _arg_type, _arg_type, _hir_type);\
\
    REGISTER_BINARY_OPERATOR(KETL_HIR_EQUAL,            _arg_type, p_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_NOT_EQUAL,        _arg_type, p_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_LESS,             _arg_type, p_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_LESS_OR_EQUAL,    _arg_type, p_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_GREATER,          _arg_type, p_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_GREATER_OR_EQUAL, _arg_type, p_bool, _hir_type);\
} while (false)

    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i8,  KETL_HIR_I8);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i16, KETL_HIR_I16);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i32, KETL_HIR_I32);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i64, KETL_HIR_I64);

    return p_state;
}

void ketl_state_destroy(ketl_state* p_state) {
    compile_function_declarations_t_deinit(&p_state->compile_function_declarations);

    for (uint32_t i = 0; i < ANN_ARRAY_SIZE(p_state->am_hiroperator_overloading); ++i) {
        operator_overloading_map_deinit(p_state->am_hiroperator_overloading + i);
    }

    KETL_HASH_MAP_FOREACH(array_types_map_t, &p_state->array_types,
        ketl_free(p_state->p_allocator, __p_bucket->value);    
    );
    array_types_map_t_deinit(&p_state->array_types);
    KETL_HASH_MAP_FOREACH(function_types_map, &p_state->function_types,
        ketl_free(p_state->p_allocator, __p_bucket->value.p_signature);    
    );
    function_types_map_deinit(&p_state->function_types);

#define FREE_PRIMITIVE_TYPE(_name)\
do {\
ketl_atomic_string s_type_name = ketl_atomic_strings_get(&p_state->atomic_strings, _name, sizeof(_name) - 1);\
ketl_namespace_node* p_type_node = ketl_namespace_find(&p_state->global_namespace, s_type_name);\
ANN_ASSERT(p_type_node->variable.kind == KETL_VARIABLE_TYPE);\
ketl_free(p_state->p_allocator, p_type_node->variable.p_pointer);\
} while(0)

    FREE_PRIMITIVE_TYPE("none");
    FREE_PRIMITIVE_TYPE("bool");
    FREE_PRIMITIVE_TYPE("char");

    FREE_PRIMITIVE_TYPE("i8");
    FREE_PRIMITIVE_TYPE("i16");
    FREE_PRIMITIVE_TYPE("i32");
    FREE_PRIMITIVE_TYPE("i64");

    ketl_modules_t_deinit(&p_state->modules);
    ketl_namespace_deinit(&p_state->global_namespace);
    ketl_executable_memory_deinit(&p_state->executable_memory);
    ketl_atomic_strings_deinit(&p_state->atomic_strings);
    string_builder_t_deinit(&p_state->error_stream);
    ketl_gc_deinit(&p_state->gc);

    ketl_free(p_state->p_allocator, p_state);
}

// TODO FIX might be called often, replace allocation on heap with field in ketl_state
ketl_type* ketl_state_get_none_type(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("none"));
}

ketl_type* ketl_state_get_raw_type(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("raw"));
}

ketl_type* ketl_state_get_i8(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("i8"));
}

ketl_type* ketl_state_get_i16(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("i16"));
}

ketl_type* ketl_state_get_i32(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("i32"));
}

ketl_type* ketl_state_get_i64(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("i64"));
}

ketl_type* ketl_state_get_type(ketl_state* p_state, const char* p_type_name, uint32_t length) {
    return ketl_state_get_type_impl(p_state, &p_state->global_namespace, p_type_name, length);
}

ketl_type* ketl_state_get_type_impl(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_type_name, uint32_t length) {
    ketl_atomic_string s_type_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_type_name, length);
    ketl_namespace_node* p_type_node = ketl_namespace_find(p_namespace, s_type_name);
    ANN_ASSERT(p_type_node->variable.kind == KETL_VARIABLE_TYPE);
    return p_type_node->variable.p_pointer;
}

ketl_type* ketl_state_get_array_type(ketl_state* p_state, ketl_type* p_type) {
    ketl_type_array* p_array_type = ketl_alloc(p_state->p_allocator, sizeof(ketl_type_array));
    *p_array_type = (ketl_type_array){
        .kind = KETL_TYPE_ARRAY,
        .align_enum = p_type->align_enum,
        .size = sizeof(void*) * 2,
        .p_value_type = p_type,
    };
    return (ketl_type*)p_array_type;
}

ketl_type* ketl_state_get_function_type(ketl_state* p_state, const ketl_function_parameters* p_parameters) {
    return (ketl_type*)get_function_type_composite(p_state, p_parameters)->p_func_type;
}

ketl_type* ketl_state_get_cfunction_type(ketl_state* p_state, const ketl_function_parameters* p_parameters) {
    return (ketl_type*)get_function_type_composite(p_state, p_parameters)->p_cfunc_type;
}

ketl_variable* ketl_state_define_var(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_name, uint32_t length, ketl_type* p_type) {
    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_name, length);
    ketl_variable namespace_variable = {
        .kind = ketl_variable_get_kind(p_type),
        .p_type = p_type,
        .uint64 = 0,
    };
    return ketl_namespace_put(p_namespace, s_name, namespace_variable, false);
}

void ketl_state_define_global_var(ketl_state* p_state, const char* p_name, uint32_t length, ketl_type* p_type, void* p_var) {
    // TODO make type a reference, otherwise can't use p_pointer
    ANN_ASSERT(false);
    ketl_variable* p_variable = ketl_state_define_var(p_state, &p_state->global_namespace, p_name, length, p_type);
    p_variable->p_pointer = p_var;
}

ketl_variable* ketl_state_define_function(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_name, uint32_t length, ketl_type* p_type, void(*cfunc)(void)) {
    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_name, length);
    ketl_function_header* p_func_header = ketl_alloc(p_state->p_allocator, sizeof(ketl_function_header));
    *p_func_header = (ketl_function_header){
        .cfunc = cfunc,
        .s_name = s_name,
    };
    ketl_variable namespace_variable = {
        .kind = ketl_variable_get_kind(p_type),
        .p_type = p_type,
        .p_func = p_func_header,
    };
    return ketl_namespace_put(p_namespace, s_name, namespace_variable, false);
}

void ketl_state_define_global_function(ketl_state* p_state, const char* p_name, uint32_t length, ketl_type* p_type, void(*cfunc)(void)) {
    ketl_state_define_function(p_state, &p_state->global_namespace, p_name, length, p_type, cfunc);
}

void ketl_state_define_global_class(ketl_state* p_state, const char* p_name, uint32_t length, ketl_named_variable_type_info_t* p_fields, uint16_t field_count) {
    ketl_symboled_variable_type_info_t* p_fields_impl = ketl_alloc(p_state->p_allocator, sizeof(ketl_symboled_variable_type_info_t) * field_count); 
    for (uint32_t i = 0u; i < field_count; ++i) {
        p_fields_impl[i].info = p_fields[i].info;
        p_fields_impl[i].s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_fields[i].p_name, p_fields[i].name_length);
    }

    ketl_type_size_pair_t class_size_pair = ketl_type_calc_class_size(p_fields_impl, field_count);
    ketl_type* p_class = ketl_alloc(p_state->p_allocator, sizeof(ketl_type_class));
    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_name, length);
    INIT_TYPE(p_class, ketl_type_class) {
            .s_name = s_name,
            .kind = KETL_TYPE_CLASS,
            .align_enum = ketl_align_find(class_size_pair.align),
            .size = class_size_pair.size,
            .fields_count = field_count,
            .methods_count = 0u,
            .p_fields = p_fields_impl,
            .p_methods = NULL,
            };
    ketl_variable namespace_variable = {
        .kind = KETL_VARIABLE_TYPE,
        .p_type = NULL,
        .p_pointer = p_class,
    };
    ketl_namespace_put(&p_state->global_namespace, s_name, namespace_variable, false);
}

void* ketl_state_compile_function(ketl_state* p_state, ketl_lexer_t* p_lexer, ketl_token_iterator_t end_pos, ketl_namespace* p_namespace, uint32_t* p_opcodes_size, ketl_named_variable_type_info_t* p_parameters, uint32_t parameter_count, ketl_variable* p_output_variable, bool print_asm) {    
    uint32_t error_stream_mark = p_state->error_stream.size;
    
    ketl_hir_t hir;
    ketl_parser_build_hir(p_state, &hir, p_lexer, end_pos, p_namespace, p_parameters, parameter_count, false, p_state->p_allocator);
    if (p_state->error_stream.size > error_stream_mark) {
        if (p_output_variable != NULL) {
            ketl_variable_set_type(p_output_variable, ketl_state_get_none_type(p_state));
        }
        return NULL;
    }
    if (p_output_variable != NULL) {
        ketl_variable_set_type(p_output_variable, hir.p_used_types[hir.return_type]);
    }

    if (false) {
        char arr_buffer[1024];
        uint32_t length = ketl_hir_format(p_state, &hir, arr_buffer, ANN_ARRAY_SIZE(arr_buffer));
        printf("%.*s", length, arr_buffer);
    }
    
#if ANN_BUILD_DEBUG
    for (uint32_t i = 0u; i < hir.vars_count; ++i) {
        if (hir.p_vars[i].type == KETL_HIR_USED_TYPE_UNKNOWN) {
            // TODO error debug only
            // cause in release we want it to finish building and show all of the errors
            ANN_ASSERT(false);
        } 
    }
#endif

    // printf("-------------------------------\n");

    ketl_asm_x86_builder_t asm_builder;
    ketl_asm_x86_builder_init(&asm_builder, p_state, KETL_ASM_X86_ABI_DEFAULT, false);
    
    ketl_asm_x86_t asm_x86;
    ketl_asm_x86_build(&hir, &asm_builder, &asm_x86, 0);

    if (print_asm) {
        char arr_buffer[4096];
        uint32_t length = ketl_asm_x86_format(p_state, &asm_x86, arr_buffer, ANN_ARRAY_SIZE(arr_buffer), true);
        printf("%.*s", length, arr_buffer);
    }
    ketl_asm_x86_builder_deinit(&asm_builder);

    // printf("-------------------------------\n");

    uint8_t* p_opcodes = ketl_asm_x86_compile(&asm_x86, p_opcodes_size, p_state->p_allocator);
    ketl_asm_x86_deinit(&asm_x86);

    return p_opcodes;
}

ketl_value* ketl_state_eval(ketl_state* p_state, const char* p_source, uint32_t length) {
    ketl_namespace local_namespace;
    ketl_namespace_init(&local_namespace, KETL_ATOMIC_STRING_EMPTY, &p_state->atomic_strings, &p_state->global_namespace, p_state->p_allocator);

    uint32_t compile_function_mark = p_state->compile_function_declarations.size;
    uint32_t error_stream_mark = p_state->error_stream.size;

    ketl_lexer_t lexer;
    ketl_lexer_init(&lexer, p_state->p_allocator);
    ketl_lexer_build_tokens(&lexer, ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("<eval>")), p_source, length);

    ketl_variable output_variable;
    uint32_t opcodes_size = 0u;
    uint8_t* p_opcodes = ketl_state_compile_function(p_state, &lexer, lexer.tokens.size, &local_namespace, &opcodes_size, NULL, 0, &output_variable, false);
    
    compile_function_declarations_t compile_function_declarations;
    compile_function_declarations_t_init(&compile_function_declarations, 4, p_state->p_allocator);
    for (uint32_t i = compile_function_mark; i < p_state->compile_function_declarations.size; ++i) {
        compile_function_declarations_t_push_back_ref(&compile_function_declarations, &p_state->compile_function_declarations.p_data[i]);
    }
    p_state->compile_function_declarations.size = compile_function_mark;

    ketl_state_postload(p_state, &lexer, &local_namespace, &compile_function_declarations, false);
    ketl_lexer_deinit(&lexer);

    if (p_state->error_stream.size > error_stream_mark) {
        // TODO return error
        fprintf(stderr, "%.*s", p_state->error_stream.size - error_stream_mark, p_state->error_stream.p_data + error_stream_mark);
        p_state->error_stream.size = error_stream_mark;
    }

    if (p_opcodes == NULL) {
        return ketl_value_from_variable(output_variable, p_state->p_allocator);
    }

    const char* p_func_name = ".eval";
    const char* p_library_filename = ".eval.dll";

    {
        uint64_t export_count = 1;
        export_info a_export[256] = {
            {
                .p_name = p_func_name,
                .p_opcodes = p_opcodes,
                .opcodes_size = opcodes_size,
            },
        };

        for (uint32_t i = compile_function_mark; i < compile_function_declarations.size; ++i) {
            ANN_ASSERT(export_count < ANN_ARRAY_SIZE(a_export));

            a_export[export_count++] = (export_info){
                .p_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, compile_function_declarations.p_data[i].s_name),
                .p_opcodes = compile_function_declarations.p_data[i].p_opcodes,
                .opcodes_size = compile_function_declarations.p_data[i].opcodes_size,
            };
        }

        export_header export = {
            .p_filename = p_library_filename,
            .p_infos = a_export,
            .count = export_count,
        };

        ketl_dynamic_library_flush_function(&export, NULL, 0);
    }

    {
        for (uint32_t i = compile_function_mark; i < compile_function_declarations.size; ++i) {
            ketl_parameters_t_deinit(&compile_function_declarations.p_data[i].v_parameters);
            ketl_free(p_state->p_allocator, compile_function_declarations.p_data[i].p_opcodes);
            compile_function_declarations.p_data[i].p_variable->cfunc = ketl_dynamic_library_load_function(p_library_filename, 
                ketl_atomic_strings_get_pointer(&p_state->atomic_strings, compile_function_declarations.p_data[i].s_name));
        }
    }
    
    compile_function_declarations_t_deinit(&compile_function_declarations);

    ketl_free(p_state->p_allocator, p_opcodes);
    uint64_t(*func)(void) = (uint64_t(*)(void))ketl_dynamic_library_load_function(p_library_filename, p_func_name);

    output_variable.uint64 = func();

    ketl_namespace_deinit(&local_namespace);

    return ketl_value_from_variable(output_variable, p_state->p_allocator);
}

void ketl_state_postload(ketl_state* p_state, ketl_lexer_t* p_lexer, ketl_namespace* p_namespace, compile_function_declarations_t* p_compile_function_declarations, bool print_asm) {
    for (uint32_t i = 0; i < p_compile_function_declarations->size; ++i) {
        ketl_namespace local_namespace;
        ketl_namespace_init(&local_namespace, KETL_ATOMIC_STRING_EMPTY, &p_state->atomic_strings, p_namespace, p_state->p_allocator);

        ketl_named_variable_type_info_t* p_function_parameters_named = p_compile_function_declarations->p_data[i].v_parameters.p_data;
        uint32_t parameters_count = p_compile_function_declarations->p_data[i].v_parameters.size;

        p_lexer->token_iterator = p_compile_function_declarations->p_data[i].start_pos;

        ketl_variable output_variable;
        uint32_t opcodes_size = 0u;
        uint8_t* p_opcodes = ketl_state_compile_function(p_state, p_lexer, p_compile_function_declarations->p_data[i].end_pos, &local_namespace, &opcodes_size, p_function_parameters_named, parameters_count, &output_variable, print_asm);

        p_compile_function_declarations->p_data[i].p_opcodes = p_opcodes;
        p_compile_function_declarations->p_data[i].opcodes_size = opcodes_size;

        ketl_namespace_deinit(&local_namespace);
    }
}

bool ketl_state_load_module(ketl_state* p_state, const char* p_module_name, uint32_t length, bool print_asm) {
    ketl_atomic_string s_module_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_module_name, length);
    return ketl_state_load_module_impl(p_state, s_module_name, NULL, print_asm);
}

bool ketl_state_load_module_impl(ketl_state* p_state, ketl_atomic_string s_module_name, ketl_namespace* p_namespace, bool print_asm) {
    const char* p_module_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, s_module_name);

    char a_module_filename[256] = {0};
    snprintf(a_module_filename, ANN_ARRAY_SIZE(a_module_filename), "%s.ktl", p_module_name);

    uint64_t module_size = p_state->modules.size;
    ketl_modules_t_bucket* p_module_bucket = ketl_modules_t_get_or_insert_copy(&p_state->modules, s_module_name, (ketl_module_t){0});
    // old insert
    if (module_size == p_state->modules.size) {
        ketl_module_preload(&p_module_bucket->value, a_module_filename, p_namespace, p_state, print_asm);
        return true;
    }

    bool top_loading = !p_state->loading_modules;
    p_state->loading_modules = true;

    ketl_module_init(&p_module_bucket->value, s_module_name, p_state);

    // preloading
    if (!ketl_module_preload(&p_module_bucket->value, a_module_filename, p_namespace, p_state, print_asm)) {
        ketl_modules_t_erase(&p_state->modules, p_module_bucket);
        return false;
    }

    if (!top_loading) {
        return true;
    }

    // postloading evetything
    KETL_HASH_MAP_FOREACH(ketl_modules_t, &p_state->modules,
        ketl_module_t* p_module = &__p_bucket->value;
        if (p_module->header_loaded && !p_module->body_loaded) {
            ketl_module_load(p_module, p_state, print_asm);
        }
    );

    p_state->loading_modules = false;

    return true;
}

static bool variable_is_function(ketl_variable* p_variable) {
    return p_variable->kind == KETL_VARIABLE_CFUNC || p_variable->kind == KETL_VARIABLE_FUNC;
}

static bool namespace_has_vars(ketl_namespace* p_namespace) {
    for (uint32_t i = 0; i < p_namespace->v_nodes.size; ++i) {
        ketl_variable* p_variable = &p_namespace->v_nodes.p_data[i].variable;
        if (!variable_is_function(p_variable)) {
            return true;
        }
    }
    return false;
}

void ketl_state_module_print_asm(ketl_state* p_state, const char* p_module_name, uint32_t module_name_length) {
    ketl_atomic_string s_module_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_module_name, module_name_length);

    uint64_t module_size = p_state->modules.size;
    ketl_modules_t_bucket* p_module_bucket = ketl_modules_t_get_or_insert_copy(&p_state->modules, s_module_name, (ketl_module_t){0});

    // old insert
    if (module_size == p_state->modules.size) {
        return;
    }

    ketl_module_t* p_module = &p_module_bucket->value;
    ketl_module_init(p_module, s_module_name, p_state);

    char a_module_filename[256] = {0};
    snprintf(a_module_filename, ANN_ARRAY_SIZE(a_module_filename), "%s.ktl", p_module_name);

    //////////////////////////////////////////

    FILE *p_module_file = fopen(a_module_filename, "rb");
    ANN_ASSERT(p_module_file != NULL);
    
    fseek(p_module_file, 0L, SEEK_END);
    int64_t filesize = ftell(p_module_file);
    ANN_ASSERT(filesize >= 0);

    p_module->p_source = ketl_alloc(p_state->p_allocator, (uint64_t)filesize);

    fseek(p_module_file, 0L, SEEK_SET);
    fread(p_module->p_source, sizeof(*p_module->p_source), filesize, p_module_file);
    ANN_ASSERT(ferror(p_module_file) == 0);

    fclose(p_module_file);

    ///////////////////////////////////////////

    uint32_t error_stream_mark = p_state->error_stream.size;

    ///////////////////////////////////////////
    
    ketl_lexer_init(&p_module->lexer, p_state->p_allocator);
    ketl_lexer_build_tokens(&p_module->lexer, ketl_atomic_strings_get(&p_state->atomic_strings, a_module_filename, KETL_NULL_TERMINATED_LENGTH_32), p_module->p_source, filesize);

    ///////////////////////////////////////////

    uint32_t compile_function_mark = p_state->compile_function_declarations.size;

    ketl_hir_t hir;
    ketl_parser_build_hir(p_state, &hir, &p_module->lexer, p_module->lexer.tokens.size, &p_module->namespace, NULL, 0, true, p_state->p_allocator);
    // for now ignoring global vars and any calls in while module is loading
    // we interested only in produced compile_function_declarations
    ketl_hir_deinit(&hir);

    /////////////////////////////////////////////

    if (p_state->error_stream.size > error_stream_mark) {
        fprintf(stderr, "%.*s", p_state->error_stream.size - error_stream_mark, p_state->error_stream.p_data + error_stream_mark);
        p_state->error_stream.size = error_stream_mark;
        return;
    }

    for (uint32_t i = compile_function_mark; i < p_state->compile_function_declarations.size; ++i) {
        compile_function_declarations_t_push_back_ref(&p_module->compile_function_declarations, &p_state->compile_function_declarations.p_data[i]);
    }

    p_state->compile_function_declarations.size = compile_function_mark;

    //////////////////////////////////////////////

    printf("    .def    @feat.00;\n");
    printf("    .scl    3;\n");
    printf("    .type   0;\n");
    printf("    .endef\n");
    printf("    .globl  @feat.00\n");
    printf("@feat.00 = 0\n");
    printf("    .intel_syntax noprefix\n");
    printf("    .file   \"%s\"\n", a_module_filename);

    ///////////////////////////////////////////////

    if (p_module->compile_function_declarations.size > 0) {
        printf("    .text\n");
    }

    for (uint32_t i = 0; i < p_module->compile_function_declarations.size; ++i) {
        compile_function_declaration_t* p_compile_function_declaration = &p_module->compile_function_declarations.p_data[i];

        ketl_namespace local_namespace;
        ketl_namespace_init(&local_namespace, KETL_ATOMIC_STRING_EMPTY, &p_state->atomic_strings, &p_module->namespace, p_state->p_allocator);

        ketl_named_variable_type_info_t* p_function_parameters_named = p_compile_function_declaration->v_parameters.p_data;
        uint32_t parameters_count = p_compile_function_declaration->v_parameters.size;

        p_module->lexer.token_iterator = p_compile_function_declaration->start_pos;

        uint32_t error_stream_mark = p_state->error_stream.size;

        //////////////////////////////////
    
        ketl_hir_t hir;
        ketl_parser_build_hir(p_state, &hir, &p_module->lexer, p_compile_function_declaration->end_pos, &local_namespace, p_function_parameters_named, parameters_count, false, p_state->p_allocator);
        
        if (p_state->error_stream.size > error_stream_mark) {
            fprintf(stderr, "%.*s", p_state->error_stream.size - error_stream_mark, p_state->error_stream.p_data + error_stream_mark);
            p_state->error_stream.size = error_stream_mark;
            continue;
        }
        
    #if ANN_BUILD_DEBUG
        for (uint32_t i = 0u; i < hir.vars_count; ++i) {
            if (hir.p_vars[i].type == KETL_HIR_USED_TYPE_UNKNOWN) {
                // TODO error debug only
                // cause in release we want it to finish building and show all of the errors
                ANN_ASSERT(false);
            } 
        }
    #endif

        /////////////////////////////////////////

        ketl_asm_x86_builder_t asm_builder;
        ketl_asm_x86_builder_init(&asm_builder, p_state, KETL_ASM_X86_ABI_DEFAULT, true);
        
        ketl_asm_x86_t asm_x86;
        ketl_asm_x86_build(&hir, &asm_builder, &asm_x86, i);
        ketl_hir_deinit(&hir);

        if (true) {
            const char* p_func_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_compile_function_declaration->s_name);

            printf("    .def    %s;\n", p_func_name);
            printf("    .scl    2;\n");
            printf("    .type   32;\n");
            printf("    .endef\n");
            printf("    .text\n");
            printf("    .globl	%s\n", p_func_name);
            printf("    .p2align	4\n");
            printf("%s:\n", p_func_name);

            char arr_buffer[4096];
            uint32_t length = ketl_asm_x86_format(p_state, &asm_x86, arr_buffer, ANN_ARRAY_SIZE(arr_buffer), false);
            printf("%.*s", length, arr_buffer);
        }
        ketl_asm_x86_builder_deinit(&asm_builder);

        /////////////////////////
        
        uint32_t opcodes_size = 0u;
        uint8_t* p_opcodes = ketl_asm_x86_compile(&asm_x86, &opcodes_size, p_state->p_allocator);
        ketl_asm_x86_deinit(&asm_x86);

        /////////////////////////

        p_compile_function_declaration->p_opcodes = p_opcodes;
        p_compile_function_declaration->opcodes_size = opcodes_size;

        ketl_namespace_deinit(&local_namespace);
    }

    ///////////////////////////////

    if (namespace_has_vars(&p_module->namespace)) {
        printf("    .bss\n");
    }

    for (uint32_t i = 0; i < p_module->namespace.v_nodes.size; ++i) {
        ketl_namespace_node* p_node = &p_module->namespace.v_nodes.p_data[i];
        if (variable_is_function(&p_node->variable)) {
            continue;
        }

        printf("    .globl    %s\n", ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_node->s_name));
        printf("    .p2align  %d, 0x0\n", p_node->variable.p_type->align_enum);
        printf("%s:\n", ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_node->s_name));
        printf("    .%dbyte   0\n", ketl_type_get_stack_size(p_node->variable.p_type));
    }

    // TODO print static data like string literals
}
