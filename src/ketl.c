//🫖ketl
#include "ketl_impl.h"

#include "compiler/lexer.h"
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

KETL_VECTOR_DEFINITION(types, ketl_type*)
KETL_VECTOR_DEFINITION(string_builder_t, char)

KETL_HASH_MAP_DEFINITION(operator_overloading_map, ketl_function_parameters, ketl_hir_tag_t, FUNC_PARAMETERS_HASH, IS_FUNC_PARAMETERS_EQUAL)

static const function_type_composite* get_function_type_composite(ketl_state* p_state, const ketl_function_parameters* p_parameters) {
    uint16_t parameters_count = p_parameters->parameters_count;
    function_types_map_bucket* p_bucket = function_types_map_get_or_insert_copy(&p_state->m_function_types, *p_parameters, (function_type_composite){NULL, NULL, NULL});
    if (p_bucket->value.p_signature == NULL) {
        uint64_t signature_size = sizeof(ketl_type_signature) + parameters_count * sizeof(ketl_type_parameter);
        uint64_t functions_offset = ANN_ALIGN_FORWARD(signature_size, _Alignof(ketl_type_function));
        uint64_t total_alloc_size = functions_offset + 2 * sizeof(ketl_type_function);
        void* p_alloc_mem = ketl_alloc(p_state->p_allocator, total_alloc_size);

        ketl_type_signature* p_signature = p_alloc_mem;
        *p_signature = (ketl_type_signature){
            .parameters_count = parameters_count
        };
        ketl_memcpy(p_signature->a_parameters, p_parameters->p_parameters, parameters_count * sizeof(ketl_type_parameter));

        p_bucket->key.p_parameters = p_signature->a_parameters;
        p_bucket->value.p_signature = p_signature;

        ketl_type_function* p_func_types = (ketl_type_function*)((char*)p_alloc_mem + functions_offset);
        *p_func_types = (ketl_type_function){
            .type = KETL_TYPE_FUNCTION,
            .align = _Alignof(void(*)(void)),
            .size = sizeof(void(*)(void)),
            .p_type_signature = p_signature
        };
        *(p_func_types + 1) = (ketl_type_function){
            .type = KETL_TYPE_CFUNCTION,
            .align = _Alignof(void(*)(void)),
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
    ketl_namespace_init(&p_state->global_namespace, p_allocator);
    function_types_map_init(&p_state->m_function_types, p_allocator);

    for (uint32_t i = 0; i < ANN_ARRAY_SIZE(p_state->am_hiroperator_overloading); ++i) {
        operator_overloading_map_init(p_state->am_hiroperator_overloading + i, p_allocator);
    }

#define INIT_TYPE(_var, _type) *(_type*)(_var) = (_type)
#define CREATE_PRIMITIVE_TYPE(_var_name, _name, _size, _is_integer, _is_signed)\
ketl_type* _var_name = ketl_alloc(p_allocator, sizeof(ketl_type_primitive)); \
do {\
ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR(_name));\
INIT_TYPE(_var_name, ketl_type_primitive) {\
        .s_name = s_name,\
        .type = KETL_TYPE_PRIMITIVE,\
        .align = _size,\
        .size = _size,\
        .is_integer = _is_integer,\
        .is_signed = _is_signed,\
        };\
ketl_variable namespace_variable = {\
    .type = KETL_VARIABLE_TYPE,\
    .p_type = NULL,\
    .pointer = _var_name,\
};\
ketl_namespace_put(&p_state->global_namespace, s_name, namespace_variable);\
} while(0)

    CREATE_PRIMITIVE_TYPE(t_none,  "none", 0, false, false);
    CREATE_PRIMITIVE_TYPE(t_bool,  "bool", 1, false, false);
    CREATE_PRIMITIVE_TYPE(t_char,  "char", 1, true,  false);

    CREATE_PRIMITIVE_TYPE(p_i8,  "i8",   1, true,  true);
    CREATE_PRIMITIVE_TYPE(p_i16, "i16",  2, true,  true);
    CREATE_PRIMITIVE_TYPE(p_i32, "i32",  4, true,  true);
    CREATE_PRIMITIVE_TYPE(p_i64, "i64",  8, true,  true);

#define REGISTER_BINARY_OPERATOR(_hir_tag_op, _arg_type, _return_type, _hir_type)\
do {\
    ketl_type_parameter parameters_array[] = { {.p_type = _return_type}, {.p_type = _arg_type}, {.p_type = _arg_type} };\
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
    REGISTER_BINARY_OPERATOR(KETL_HIR_EQUAL,            _arg_type, t_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_NOT_EQUAL,        _arg_type, t_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_LESS,             _arg_type, t_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_LESS_OR_EQUAL,    _arg_type, t_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_GREATER,          _arg_type, t_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_GREATER_OR_EQUAL, _arg_type, t_bool, _hir_type);\
} while (false)

    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i8,  KETL_HIR_I8);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i16, KETL_HIR_I16);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i32, KETL_HIR_I32);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i64, KETL_HIR_I64);

    return p_state;
}

void ketl_state_destroy(ketl_state* p_state) {
    for (uint32_t i = 0; i < ANN_ARRAY_SIZE(p_state->am_hiroperator_overloading); ++i) {
        operator_overloading_map_deinit(p_state->am_hiroperator_overloading + i);
    }

    KETL_HASH_MAP_FOREACH(function_types_map, &p_state->m_function_types,
        ketl_free(p_state->p_allocator, __p_bucket->value.p_signature);    
    );
    function_types_map_deinit(&p_state->m_function_types);

#define FREE_PRIMITIVE_TYPE(_name)\
do {\
ketl_atomic_string s_type_name = ketl_atomic_strings_get(&p_state->atomic_strings, _name, sizeof(_name) - 1);\
ketl_namespace_node* p_type_node = ketl_namespace_find(&p_state->global_namespace, s_type_name);\
ANN_ASSERT(p_type_node->variable.type == KETL_VARIABLE_TYPE);\
ketl_free(p_state->p_allocator, p_type_node->variable.pointer);\
} while(0)

    FREE_PRIMITIVE_TYPE("none");
    FREE_PRIMITIVE_TYPE("bool");
    FREE_PRIMITIVE_TYPE("char");

    FREE_PRIMITIVE_TYPE("i8");
    FREE_PRIMITIVE_TYPE("i16");
    FREE_PRIMITIVE_TYPE("i32");
    FREE_PRIMITIVE_TYPE("i64");

    ketl_namespace_deinit(&p_state->global_namespace);
    ketl_atomic_strings_deinit(&p_state->atomic_strings);
    string_builder_t_deinit(&p_state->error_stream);
    ketl_gc_deinit(&p_state->gc);

    ketl_free(p_state->p_allocator, p_state);
}

// TODO FIX might be called often, replace allocation on heap with field in ketl_state
ketl_type* ketl_state_get_none_type(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("none"));
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
    ketl_atomic_string a_type_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_type_name, length);
    ketl_namespace_node* p_type_node = ketl_namespace_find(&p_state->global_namespace, a_type_name);
    ANN_ASSERT(p_type_node->variable.type == KETL_VARIABLE_TYPE);
    return p_type_node->variable.pointer;
}

ketl_type* ketl_state_get_function_type(ketl_state* p_state, const ketl_function_parameters* p_parameters) {
    return (ketl_type*)get_function_type_composite(p_state, p_parameters)->p_func_type;
}

ketl_type* ketl_state_get_cfunction_type(ketl_state* p_state, const ketl_function_parameters* p_parameters) {
    return (ketl_type*)get_function_type_composite(p_state, p_parameters)->p_cfunc_type;
}

void ketl_state_define_function(ketl_state* p_state, const char* p_name, uint32_t length, ketl_type* p_type, void* p_func) {
    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_name, length);
    ketl_variable namespace_variable = {
        .type = KETL_VARIABLE_POINTER,
        .p_type = p_type,
        .pointer = p_func,
    };
    ketl_namespace_put(&p_state->global_namespace, s_name, namespace_variable);
}

void ketl_state_define_class(ketl_state* p_state, const char* p_name, uint32_t length, ketl_class_field* p_fields, uint16_t field_count) {
    ketl_class_field_impl* p_fields_impl = ketl_alloc(p_state->p_allocator, sizeof(ketl_class_field_impl) * field_count); 
    for (uint32_t i = 0u; i < field_count; ++i) {
        p_fields_impl[i].p_type.p_type = p_fields[i].p_type;
        p_fields_impl[i].s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_fields[i].p_name, KETL_NULL_TERMINATED_LENGTH_32);
    }

    ketl_type_size_pair_t class_size_pair = ketl_type_calc_class_size(p_fields_impl, field_count);
    ketl_type* p_class = ketl_alloc(p_state->p_allocator, sizeof(ketl_type_class));
    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_name, length);
    INIT_TYPE(p_class, ketl_type_class) {
            .s_name = s_name,
            .type = KETL_TYPE_CLASS,
            .align = class_size_pair.align,
            .size = class_size_pair.size,
            .fields_count = field_count,
            .methods_count = 0u,
            .p_fields = p_fields_impl,
            .p_methods = NULL,
            };
    ketl_variable namespace_variable = {
        .type = KETL_VARIABLE_TYPE,
        .p_type = NULL,
        .pointer = p_class,
    };
    ketl_namespace_put(&p_state->global_namespace, s_name, namespace_variable);
}

ketl_value* ketl_state_eval(ketl_state* p_state, const char* p_filename, const char* p_source, uint32_t length) {
    ketl_variable output_variable;

    ////////////////////////////////

    ketl_lexer_t lexer;
    ketl_lexer_init(&lexer, p_state->p_allocator);
    ketl_lexer_build_tokens(&lexer, p_source, length);

    ////////////////////////////////

    ketl_hir_t hir;
    ketl_parser_build_hir(p_state, &hir, p_filename, &lexer, p_state->p_allocator);
    ketl_lexer_deinit(&lexer);
    if (p_state->error_stream.size > 0) {
        // TODO return error
        printf("%.*s", p_state->error_stream.size, p_state->error_stream.p_data);
        p_state->error_stream.size = 0;

        ketl_variable_set_type(&output_variable, ketl_state_get_none_type(p_state));
        return ketl_value_from_variable(output_variable, p_state->p_allocator);
    }
    ketl_variable_set_type(&output_variable, hir.p_used_types[hir.return_type]);

    {
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

    printf("-------------------------------\n");

    ketl_asm_x86_builder_t asm_builder;
    ketl_asm_x86_builder_init(&asm_builder, p_state->p_allocator, KETL_ASM_X86_ABI_DEFAULT);
    
    ketl_asm_x86_t asm_x86;
    ketl_asm_x86_build(p_state, &hir, &asm_builder, &asm_x86);

    {
        char arr_buffer[2048];
        uint32_t length = ketl_asm_x86_format(&asm_x86, arr_buffer, ANN_ARRAY_SIZE(arr_buffer));
        printf("%.*s", length, arr_buffer);
    }
    ketl_asm_x86_builder_deinit(&asm_builder);

    printf("-------------------------------\n");

    uint32_t opcodes_size = 0u;
    uint8_t* p_opcodes = ketl_asm_x86_compile(&asm_x86, &opcodes_size, p_state->p_allocator);
    ketl_asm_x86_deinit(&asm_x86);

    ketl_executable_memory ex_memory;
    ketl_executable_memory_init(&ex_memory, p_state->p_allocator);

#if 1
    uint8_t* executable_opcodes = ketl_executable_memory_allocate(&ex_memory, p_opcodes, opcodes_size);
    ketl_free(p_state->p_allocator, p_opcodes);

    {
        char arr_buffer[2048];
        printf("at location 0x%08"PRIx64" of length %"PRIu32"\n", (uint64_t)executable_opcodes, opcodes_size);
        uint32_t length = ketl_asm_x86_format_opcodes(executable_opcodes, opcodes_size, arr_buffer, ANN_ARRAY_SIZE(arr_buffer));
        printf("%.*s\n", length, arr_buffer);
    }

    uint64_t(*func)(void);
    #define KETL_POINTER_CONVERTER
    #define KETL_POINTER_CONVERTER_ARG  executable_opcodes
    #define KETL_POINTER_CONVERTER_VAR  func
    #define KETL_POINTER_CONVERTER_TYPE uint64_t(*)(void)
    #include "meta.i"

    output_variable.uint64 = func();
#else
    ketl_dynamic_library_flush_function("temp.dll", "<eval>", p_opcodes, opcodes_size);
    ketl_free(p_state->p_allocator, p_opcodes);

    uint64_t(*func)(void) = (uint64_t(*)(void))ketl_dynamic_library_load_function("temp.dll", "<eval>");

    output_variable.uint64 = func();
#endif

    ketl_executable_memory_deinit(&ex_memory);

    p_state->error_stream.size = 0;

    return ketl_value_from_variable(output_variable, p_state->p_allocator);
}
