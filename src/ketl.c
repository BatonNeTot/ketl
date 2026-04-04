//🫖ketl
#include "ketl_impl.h"

#include "compiler/parser.h"
#include "compiler/assembler.h"
#include "compiler/assembler_builder.h"
#include "compiler/assembler/x86.h"

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

static void array_clear(ketl_array* p_array) {
    if (p_array->is_slice) {
        p_array->p_data = NULL;
        p_array->capacity = 0;
        p_array->size = 0;
        p_array->is_slice = false;
    } else {
        p_array->size = 0;
    }
}

#include <stdlib.h>
#include <string.h>
static const char* str2cstr(ketl_array* p_str) {
    if (!p_str) {
        return NULL;
    }

    char* p_cstr = malloc(p_str->size + 1);
    memcpy(p_cstr, p_str->p_data, p_str->size);
    p_cstr[p_str->size] = '\0';
    return p_cstr;
}

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
    ketl_namespace_init(&p_state->secret_namespace, KETL_ATOMIC_STRING_EMPTY, &p_state->atomic_strings, NULL, p_allocator);
    ketl_modules_t_init(&p_state->modules, p_allocator);

    function_types_map_init(&p_state->function_types, p_allocator);
    array_types_map_t_init(&p_state->array_types, p_allocator);

    for (uint32_t i = 0; i < ANN_ARRAY_SIZE(p_state->am_hiroperator_overloading); ++i) {
        operator_overloading_map_init(p_state->am_hiroperator_overloading + i, p_allocator);
    }

    compile_function_declarations_t_init(&p_state->compile_function_declarations, 16, p_allocator);
    p_state->loading_modules = false;
    p_state->active_module_name = KETL_ATOMIC_STRING_EMPTY;

#define INIT_TYPE(_var, _type) *(_type*)(_var) = (_type)
#define CREATE_PRIMITIVE_TYPE(_var_name, _name, _size, _is_integer, _is_signed, _is_numeric)\
ketl_type* _var_name = ketl_alloc(p_allocator, sizeof(ketl_type_primitive)); \
do {\
ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR(_name));\
INIT_TYPE(_var_name, ketl_type_primitive) {\
        .kind = KETL_TYPE_PRIMITIVE,\
        .align_enum = ketl_align_find(_size),\
        .size = _size,\
        .is_integer = _is_integer,\
        .is_signed = _is_signed,\
        .is_numeric = _is_numeric,\
        };\
ketl_variable namespace_variable = {\
    .kind = KETL_VARIABLE_TYPE,\
    .p_type = NULL,\
    .p_pointer = _var_name,\
};\
ketl_namespace_put(&p_state->global_namespace, s_name, KETL_ATOMIC_STRING_EMPTY, namespace_variable, (ketl_namespace_node_info){0}, &p_state->atomic_strings, false);\
} while(0)

    CREATE_PRIMITIVE_TYPE(p_none,  "none", 0, false, false, false);
    CREATE_PRIMITIVE_TYPE(p_bool,  "bool", 1, false, false, false);
    CREATE_PRIMITIVE_TYPE(p_char,  "char", 1, true,  false, false);
    CREATE_PRIMITIVE_TYPE(p_raw,   "raw",  8, false, false, false);
    
    ketl_type* p_str;
    {
        ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("str"));
        ketl_variable namespace_variable = {
            .kind = KETL_VARIABLE_TYPE,
            .p_type = NULL,
            .p_pointer = p_str = ketl_state_get_array_type(p_state, p_char),
        };
        ketl_namespace_put(&p_state->global_namespace, s_name, KETL_ATOMIC_STRING_EMPTY, namespace_variable, (ketl_namespace_node_info){0}, &p_state->atomic_strings, false);
    }

    ketl_type* p_cstr;
    {
        ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("cstr"));
        ketl_variable namespace_variable = {
            .kind = KETL_VARIABLE_TYPE,
            .p_type = NULL,
            .p_pointer = p_cstr = ketl_state_get_carray_type(p_state, p_char),
        };
        ketl_namespace_put(&p_state->global_namespace, s_name, KETL_ATOMIC_STRING_EMPTY, namespace_variable, (ketl_namespace_node_info){0}, &p_state->atomic_strings, false);
    }
    
    CREATE_PRIMITIVE_TYPE(p_i8,  "i8",   1, true,  true,  true);
    CREATE_PRIMITIVE_TYPE(p_i16, "i16",  2, true,  true,  true);
    CREATE_PRIMITIVE_TYPE(p_i32, "i32",  4, true,  true,  true);
    CREATE_PRIMITIVE_TYPE(p_i64, "i64",  8, true,  true,  true);
    
    CREATE_PRIMITIVE_TYPE(p_u8,  "u8",   1, true,  false, true);
    CREATE_PRIMITIVE_TYPE(p_u16, "u16",  2, true,  false, true);
    CREATE_PRIMITIVE_TYPE(p_u32, "u32",  4, true,  false, true);
    CREATE_PRIMITIVE_TYPE(p_u64, "u64",  8, true,  false, true);

#define REGISTER_UNARY_OPERATOR(_hir_tag_op, _arg_type, _return_type, _hir_type)\
do {\
    ketl_variable_type_info_t parameters_array[] = { {.p_type = _return_type}, {.p_type = _arg_type} };\
    ketl_function_parameters parameters = {\
        .p_parameters = parameters_array,\
        .parameters_count = sizeof(parameters_array) / sizeof(*parameters_array)\
    };\
\
    const function_type_composite* p_func_type_composite = get_function_type_composite(p_state, &parameters);\
    parameters.p_parameters = p_func_type_composite->p_signature->a_parameters;\
\
    operator_overloading_map_get_or_insert_copy(p_state->am_hiroperator_overloading + \
        ((_hir_tag_op - KETL_HIR_FIRST_OVERLOADABLE_OPERATOR) >> KETL_HIR_TYPE_INSTR_SHIFT), parameters, _hir_tag_op | _hir_type);\
} while (false)

#define REGISTER_BINARY_OPERATOR(_hir_tag_op, _lhs_arg_type, _rhs_arg_type, _return_type, _hir_type)\
do {\
    ketl_variable_type_info_t parameters_array[] = { {.p_type = _return_type}, {.p_type = _lhs_arg_type}, {.p_type = _rhs_arg_type} };\
    ketl_function_parameters parameters = {\
        .p_parameters = parameters_array,\
        .parameters_count = sizeof(parameters_array) / sizeof(*parameters_array)\
    };\
\
    const function_type_composite* p_func_type_composite = get_function_type_composite(p_state, &parameters);\
    parameters.p_parameters = p_func_type_composite->p_signature->a_parameters;\
\
    operator_overloading_map_get_or_insert_copy(p_state->am_hiroperator_overloading + \
        ((_hir_tag_op - KETL_HIR_FIRST_OVERLOADABLE_OPERATOR) >> KETL_HIR_TYPE_INSTR_SHIFT), parameters, _hir_tag_op | _hir_type);\
} while (false)

    REGISTER_UNARY_OPERATOR(KETL_HIR_LOGICAL_NOT, p_bool, p_bool, KETL_HIR_U8);
    REGISTER_UNARY_OPERATOR(KETL_HIR_LOGICAL_NOT, p_char, p_bool, KETL_HIR_I8);
    REGISTER_UNARY_OPERATOR(KETL_HIR_LOGICAL_NOT, p_raw,  p_bool, KETL_HIR_U64);

    REGISTER_UNARY_OPERATOR(KETL_HIR_LOGICAL_NOT, p_i8,   p_bool, KETL_HIR_I8);
    REGISTER_UNARY_OPERATOR(KETL_HIR_LOGICAL_NOT, p_i16,  p_bool, KETL_HIR_I16);
    REGISTER_UNARY_OPERATOR(KETL_HIR_LOGICAL_NOT, p_i32,  p_bool, KETL_HIR_I32);
    REGISTER_UNARY_OPERATOR(KETL_HIR_LOGICAL_NOT, p_i64,  p_bool, KETL_HIR_I64);
    
    REGISTER_UNARY_OPERATOR(KETL_HIR_LOGICAL_NOT, p_u8,   p_bool, KETL_HIR_U8);
    REGISTER_UNARY_OPERATOR(KETL_HIR_LOGICAL_NOT, p_u16,  p_bool, KETL_HIR_U16);
    REGISTER_UNARY_OPERATOR(KETL_HIR_LOGICAL_NOT, p_u32,  p_bool, KETL_HIR_U32);
    REGISTER_UNARY_OPERATOR(KETL_HIR_LOGICAL_NOT, p_u64,  p_bool, KETL_HIR_U64);


    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_PLUS, p_char, p_char,  KETL_HIR_I8);

    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_PLUS, p_i8,   p_i8,  KETL_HIR_I8);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_PLUS, p_i16,  p_i16, KETL_HIR_I16);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_PLUS, p_i32,  p_i32, KETL_HIR_I32);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_PLUS, p_i64,  p_i64, KETL_HIR_I64);
    
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_PLUS, p_u8,   p_u8,  KETL_HIR_U8);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_PLUS, p_u16,  p_u16, KETL_HIR_U16);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_PLUS, p_u32,  p_u32, KETL_HIR_U32);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_PLUS, p_u64,  p_u64, KETL_HIR_U64);

    
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_MINUS, p_char, p_char,  KETL_HIR_I8);

    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_MINUS, p_i8,   p_i8,  KETL_HIR_I8);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_MINUS, p_i16,  p_i16, KETL_HIR_I16);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_MINUS, p_i32,  p_i32, KETL_HIR_I32);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_MINUS, p_i64,  p_i64, KETL_HIR_I64);
    
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_MINUS, p_u8,   p_i8,  KETL_HIR_I8);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_MINUS, p_u16,  p_i16, KETL_HIR_I16);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_MINUS, p_u32,  p_i32, KETL_HIR_I32);
    REGISTER_UNARY_OPERATOR(KETL_HIR_UNARY_MINUS, p_u64,  p_i64, KETL_HIR_I64);
    
#define REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(_hir_tag_op, _arg_type, _return_type, _hir_type)\
    REGISTER_BINARY_OPERATOR(_hir_tag_op, _arg_type, _arg_type, _return_type, _hir_type)

#define REGISTER_PRIMITIVE_BINARY_OPERATORS(_arg_type, _hir_type)\
do {\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_PLUS,             _arg_type, _arg_type, _hir_type);\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_MINUS,            _arg_type, _arg_type, _hir_type);\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_MULTY,            _arg_type, _arg_type, _hir_type);\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_DIV,              _arg_type, _arg_type, _hir_type);\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_MOD,              _arg_type, _arg_type, _hir_type);\
\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_BITWISE_AND,      _arg_type, _arg_type, _hir_type);\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_BITWISE_OR,       _arg_type, _arg_type, _hir_type);\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_BITWISE_XOR,      _arg_type, _arg_type, _hir_type);\
\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_EQUAL,            _arg_type, p_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_NOT_EQUAL,        _arg_type, p_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_LESS,             _arg_type, p_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_LESS_OR_EQUAL,    _arg_type, p_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_GREATER,          _arg_type, p_bool, _hir_type);\
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_GREATER_OR_EQUAL, _arg_type, p_bool, _hir_type);\
} while (false)

    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_char,  KETL_HIR_I8);

    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i8,  KETL_HIR_I8);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i16, KETL_HIR_I16);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i32, KETL_HIR_I32);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_i64, KETL_HIR_I64);

    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_u8,  KETL_HIR_U8);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_u16, KETL_HIR_U16);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_u32, KETL_HIR_U32);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(p_u64, KETL_HIR_U64);

    REGISTER_BINARY_OPERATOR(KETL_HIR_PLUS,   p_raw, p_u64, p_raw, KETL_HIR_U64);
    REGISTER_BINARY_OPERATOR(KETL_HIR_PLUS,   p_u64, p_raw, p_raw, KETL_HIR_U64);
    REGISTER_BINARY_OPERATOR(KETL_HIR_MINUS,  p_raw, p_u64, p_raw, KETL_HIR_U64);
    REGISTER_BINARY_OPERATOR(KETL_HIR_MINUS,  p_u64, p_raw, p_raw, KETL_HIR_U64);

    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_EQUAL,     p_raw, p_bool, KETL_HIR_U64);
    REGISTER_BINARY_OPERATOR_EQUAL_ARG_TYPES(KETL_HIR_NOT_EQUAL, p_raw, p_bool, KETL_HIR_U64);

    {
        ketl_variable_type_info_t a_parameters[] = {
            { .p_type = p_none },
            { .p_type = p_raw },
        };
        ketl_function_parameters clear_func_params = {
            .p_parameters = a_parameters,
            .parameters_count = ANN_ARRAY_SIZE(a_parameters),
        };
        ketl_type* p_func_type = ketl_state_get_cfunction_type(p_state, &clear_func_params);
        ketl_atomic_string s_key = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("array_clear"));
        ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("__ketl_rt.array_clear"));
        ketl_state_define_cfunction(p_state, &p_state->secret_namespace, s_key, s_name, p_func_type, (void(*)(void))&array_clear, false);
    }

    {
        ketl_variable_type_info_t a_parameters[] = {
            { .p_type = p_cstr },
            { .p_type = p_str },
        };
        ketl_function_parameters clear_func_params = {
            .p_parameters = a_parameters,
            .parameters_count = ANN_ARRAY_SIZE(a_parameters),
        };
        ketl_type* p_func_type = ketl_state_get_cfunction_type(p_state, &clear_func_params);
        ketl_atomic_string s_key = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("str2cstr"));
        ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("__ketl_rt.str2cstr"));
        ketl_state_define_cfunction(p_state, &p_state->secret_namespace, s_key, s_name, p_func_type, (void(*)(void))&str2cstr, false);
    }

    {
        ketl_variable_type_info_t a_parameters[] = {
            { .p_type = p_str },
            { .p_type = p_i64 },
        };
        ketl_function_parameters clear_func_params = {
            .p_parameters = a_parameters,
            .parameters_count = ANN_ARRAY_SIZE(a_parameters),
        };
        ketl_type* p_func_type = ketl_state_get_cfunction_type(p_state, &clear_func_params);
        ketl_atomic_string s_key = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("int2str"));
        ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("__ketl_rt.int2str"));
        ketl_state_define_cfunction(p_state, &p_state->secret_namespace, s_key, s_name, p_func_type, (void(*)(void))NULL, false);
    }

    {
        ketl_variable_type_info_t a_parameters[] = {
            { .p_type = p_none },
            { .p_type = p_bool },
        };
        ketl_function_parameters clear_func_params = {
            .p_parameters = a_parameters,
            .parameters_count = ANN_ARRAY_SIZE(a_parameters),
        };
        ketl_type* p_func_type = ketl_state_get_cfunction_type(p_state, &clear_func_params);
        ketl_atomic_string s_key = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("assert"));
        ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("__ketl_rt.assert"));
        ketl_state_define_cfunction(p_state, &p_state->global_namespace, s_key, s_name, p_func_type, (void(*)(void))&array_clear, false);
    }

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
    FREE_PRIMITIVE_TYPE("raw");

    FREE_PRIMITIVE_TYPE("i8");
    FREE_PRIMITIVE_TYPE("i16");
    FREE_PRIMITIVE_TYPE("i32");
    FREE_PRIMITIVE_TYPE("i64");

    ketl_modules_t_deinit(&p_state->modules);
    ketl_namespace_deinit(&p_state->secret_namespace);
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

ketl_type* ketl_state_get_bool_type(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("bool"));
}

ketl_type* ketl_state_get_char_type(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("char"));
}

ketl_type* ketl_state_get_raw_type(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("raw"));
}

ketl_type* ketl_state_get_str_type(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("str"));
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

ketl_type* ketl_state_get_u8(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("u8"));
}

ketl_type* ketl_state_get_u16(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("u16"));
}

ketl_type* ketl_state_get_u32(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("u32"));
}

ketl_type* ketl_state_get_u64(ketl_state* p_state) {
    return ketl_state_get_type(p_state, LITERAL_STRING_PAIR("u64"));
}

ketl_type* ketl_state_get_type(ketl_state* p_state, const char* p_type_name, uint32_t length) {
    return ketl_state_get_type_impl(p_state, &p_state->global_namespace, p_type_name, length);
}

ketl_type* ketl_state_get_type_impl(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_type_name, uint32_t length) {
    ketl_atomic_string s_type_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_type_name, length);
    ketl_namespace_node* p_type_node = ketl_namespace_find(p_namespace, s_type_name);
    return p_type_node && p_type_node->variable.kind == KETL_VARIABLE_TYPE ? p_type_node->variable.p_pointer : NULL;
}

ketl_type* ketl_state_get_array_type(ketl_state* p_state, ketl_type* p_type) {
    ketl_type_array* p_array_type = ketl_alloc(p_state->p_allocator, sizeof(ketl_type_array));
    *p_array_type = (ketl_type_array){
        .kind = KETL_TYPE_ARRAY,
        .align_enum = p_type->align_enum,
        .size = sizeof(ketl_array),
        .p_value_type = p_type,
    };
    return (ketl_type*)p_array_type;
}

ketl_type* ketl_state_get_carray_type(ketl_state* p_state, ketl_type* p_type) {
    ketl_type_array* p_array_type = ketl_alloc(p_state->p_allocator, sizeof(ketl_type_array));
    *p_array_type = (ketl_type_array){
        .kind = KETL_TYPE_CARRAY,
        .align_enum = p_type->align_enum,
        .size = sizeof(void*),
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

ketl_namespace_node* ketl_state_define_var(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_name, uint32_t length, ketl_type* p_type, bool export) {
    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_name, length);
    ketl_variable namespace_variable = {
        .kind = ketl_variable_get_kind(p_type),
        .p_type = p_type,
        .uint64 = 0,
    };
    return ketl_namespace_put(p_namespace, s_name, KETL_ATOMIC_STRING_EMPTY, namespace_variable, (ketl_namespace_node_info){.export = export}, &p_state->atomic_strings, false);
}

void ketl_state_define_global_var(ketl_state* p_state, const char* p_name, uint32_t length, ketl_type* p_type, void* p_var) {
    // TODO make type a reference, otherwise can't use p_pointer
    ANN_ASSERT(false);
    ketl_namespace_node* p_namespace_node = ketl_state_define_var(p_state, &p_state->global_namespace, p_name, length, p_type, false);
    p_namespace_node->variable.p_pointer = p_var;
}

ketl_namespace_node* ketl_state_define_cfunction(ketl_state* p_state, ketl_namespace* p_namespace, ketl_atomic_string s_key, ketl_atomic_string s_name, ketl_type* p_type, void(*cfunc)(void), bool export) {
    ketl_function_header* p_func_header = ketl_alloc(p_state->p_allocator, sizeof(ketl_function_header));
    *p_func_header = (ketl_function_header){
        .cfunc = cfunc,
    };
    ketl_variable namespace_variable = {
        .kind = ketl_variable_get_kind(p_type),
        .p_type = p_type,
        .p_func = p_func_header,
    };
    return ketl_namespace_put(p_namespace, s_key, s_name, namespace_variable, (ketl_namespace_node_info){.export = export, }, &p_state->atomic_strings, false);
}

void ketl_state_define_global_cfunction(ketl_state* p_state, const char* p_name, uint32_t length, ketl_type* p_type, void(*cfunc)(void)) {
    ketl_state_define_cfunction(p_state, &p_state->global_namespace, 
        ketl_atomic_strings_get(&p_state->atomic_strings, p_name, length), KETL_ATOMIC_STRING_EMPTY, p_type, cfunc, false);
}

#include "str.h"

ketl_namespace_node* ketl_state_forward_define_class(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_name, uint32_t length, ketl_namespace** pp_class_namespace, bool export) {
    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_name, length);
    ketl_variable namespace_variable = {
        .kind = KETL_VARIABLE_TYPE,
        .p_type = NULL,
        .p_pointer = NULL,
    };
    ketl_namespace_node* p_class_node = ketl_namespace_put(p_namespace, s_name, KETL_ATOMIC_STRING_EMPTY, namespace_variable, (ketl_namespace_node_info){.export = export}, &p_state->atomic_strings, false);
    ANN_ASSERT(p_class_node->variable.p_pointer == NULL);

    ketl_type* p_class =  ketl_alloc(p_state->p_allocator, sizeof(ketl_type_class));
    p_class_node->variable.p_pointer = p_class;

    INIT_TYPE(p_class, ketl_type_class) {
            .s_name = s_name,
            .kind = KETL_TYPE_CLASS,
            .align_enum = 0,
            .size = 0,
            .fields_count = 0,
            .p_fields = NULL,
            };
    if (pp_class_namespace != NULL) {
        *pp_class_namespace = &((ketl_type_class*)p_class)->namespace;
    }
    ketl_namespace_init(&((ketl_type_class*)p_class)->namespace, s_name, &p_state->atomic_strings, p_namespace, p_state->p_allocator);

    return p_class_node;
}

void ketl_state_post_define_class(ketl_state* p_state, ketl_namespace_node* p_class_node, ketl_named_variable_type_info_t* p_fields, uint16_t field_count) {   
    ketl_type_class* p_class_type = p_class_node->variable.p_pointer;

    ketl_symboled_variable_type_info_t* p_fields_copy = ketl_alloc(p_state->p_allocator, sizeof(ketl_symboled_variable_type_info_t) * field_count); 
    for (uint32_t i = 0u; i < field_count; ++i) {
        p_fields_copy[i].info = p_fields[i].info;
        p_fields_copy[i].s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_fields[i].p_name, p_fields[i].name_length);

        if (p_fields[i].p_name == NULL) {
            ketl_type* p_unpacked_type = p_fields[i].info.p_type;
            ANN_ASSERT(p_unpacked_type->kind == KETL_TYPE_CLASS);
            ketl_type_class* p_unpacked_class = (ketl_type_class*)p_unpacked_type;
            ketl_namespace* p_unpacked_namespace = &p_unpacked_class->namespace;

            for (uint32_t i = 0; i < p_unpacked_namespace->v_nodes.size; ++i) {
                ketl_namespace_node* p_unpacked_node = &p_unpacked_namespace->v_nodes.p_data[i];
                ketl_namespace_node_info info = p_unpacked_node->info;
                info.imported = true;
                ketl_namespace_put(&p_class_type->namespace, p_unpacked_node->s_key, p_unpacked_node->s_name, p_unpacked_node->variable, info, &p_state->atomic_strings, false);
            }
        }
    }

    ketl_type_size_pair_t class_size_pair = ketl_type_calc_class_size(p_fields_copy, field_count);
    p_class_type->align_enum = ketl_align_find(class_size_pair.align);
    p_class_type->size = class_size_pair.size;
    p_class_type->fields_count = field_count;
    p_class_type->p_fields = p_fields_copy;
}

ketl_namespace_node* ketl_state_define_enum(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_name, uint32_t length, ketl_type_primitive* p_parent_primitive, ketl_type_enum_pair* p_constants, uint64_t constant_count, bool export) {
    ketl_type_enum_pair* p_constants_impl = ketl_alloc(p_state->p_allocator, sizeof(ketl_type_enum_pair) * constant_count); 
    ketl_memcpy(p_constants_impl, p_constants, sizeof(ketl_type_enum_pair) * constant_count);

    ketl_type* p_enum = ketl_alloc(p_state->p_allocator, sizeof(ketl_type_enum));
    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_name, length);
    INIT_TYPE(p_enum, ketl_type_enum) {
            .s_name = s_name,
            .kind = KETL_TYPE_ENUM,
            .align_enum = p_parent_primitive->align_enum,
            .size = p_parent_primitive->size,
            .p_parent_primitive = p_parent_primitive,
            .constants_count = constant_count,
            .p_contants = p_constants_impl,
            };
    ketl_variable namespace_variable = {
        .kind = KETL_VARIABLE_TYPE,
        .p_type = NULL,
        .p_pointer = p_enum,
    };
    return ketl_namespace_put(p_namespace, s_name, KETL_ATOMIC_STRING_EMPTY, namespace_variable, (ketl_namespace_node_info){.export = export}, &p_state->atomic_strings, false);
}

// TODO must create a full copy of a type - so they will be same, but distinct
// for now works just like alias and would work only with primitives
ketl_namespace_node* ketl_state_define_mimic(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_name, uint32_t length, ketl_type* p_type, bool export) {
    ANN_ASSERT(p_type && p_type->kind == KETL_TYPE_PRIMITIVE);

    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_name, length);
    ketl_variable namespace_variable = {
        .kind = KETL_VARIABLE_TYPE,
        .p_type = NULL,
        .p_pointer = p_type,
    };
    return ketl_namespace_put(p_namespace, s_name, KETL_ATOMIC_STRING_EMPTY, namespace_variable, (ketl_namespace_node_info){.export = export}, &p_state->atomic_strings, false);
}

void ketl_state_define_global_class(ketl_state* p_state, const char* p_name, uint32_t length, ketl_named_variable_type_info_t* p_fields, uint16_t field_count) {
    ketl_namespace_node* p_class_node = ketl_state_forward_define_class(p_state, &p_state->global_namespace, p_name, length, NULL, false);
    ketl_state_post_define_class(p_state, p_class_node, p_fields, field_count);
}

void* ketl_state_compile_function(ketl_state* p_state, ketl_lexer_t* p_lexer, ketl_token_iterator_t end_pos, ketl_namespace* p_namespace, uint32_t* p_opcodes_size, ketl_named_variable_type_info_t* p_parameters, uint32_t parameter_count, ketl_type* p_return_type, uint16_t func_index, bool is_global_scope, ketl_variable* p_output_variable) {    
    uint32_t error_stream_mark = p_state->error_stream.size;
    
    ketl_hir_t hir;
    ketl_parser_build_hir(p_state, &hir, p_lexer, end_pos, p_namespace, p_parameters, parameter_count, p_return_type, func_index, is_global_scope, p_state->p_allocator);
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

    if (false) {
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
    ketl_atomic_string s_eval_name = ketl_atomic_strings_get(&p_state->atomic_strings, LITERAL_STRING_PAIR("<eval>"));

    ketl_namespace local_namespace;
    ketl_namespace_init(&local_namespace, s_eval_name, &p_state->atomic_strings, &p_state->global_namespace, p_state->p_allocator);

    uint32_t compile_function_mark = p_state->compile_function_declarations.size;
    uint32_t error_stream_mark = p_state->error_stream.size;

    ketl_lexer_t lexer;
    ketl_lexer_init(&lexer, p_state->p_allocator);
    ketl_lexer_build_tokens(&lexer, s_eval_name, p_source, length);

    ketl_variable output_variable;
    uint32_t opcodes_size = 0u;
    uint8_t* p_opcodes = ketl_state_compile_function(p_state, &lexer, lexer.tokens.size, &local_namespace, &opcodes_size, NULL, 0, NULL, 0, true, &output_variable);
    
    compile_function_declarations_t compile_function_declarations;
    compile_function_declarations_t_init(&compile_function_declarations, 4, p_state->p_allocator);
    for (uint32_t i = compile_function_mark; i < p_state->compile_function_declarations.size; ++i) {
        compile_function_declarations_t_push_back_ref(&compile_function_declarations, &p_state->compile_function_declarations.p_data[i]);
    }
    p_state->compile_function_declarations.size = compile_function_mark;

    ketl_state_postload(p_state, &lexer, &local_namespace, &compile_function_declarations);
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

            compile_function_declaration_t* p_compile_function_declaration = &compile_function_declarations.p_data[i];
            ketl_namespace_node* p_node = ketl_namespace_find_by_index(
                p_compile_function_declaration->p_namespace, p_compile_function_declaration->namespace_node_index);
            a_export[export_count++] = (export_info){
                .p_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_node->s_name),
                .p_opcodes = p_compile_function_declaration->p_opcodes,
                .opcodes_size = p_compile_function_declaration->opcodes_size,
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
            compile_function_declaration_t* p_compile_function_declaration = &compile_function_declarations.p_data[i];
            ketl_parameters_t_deinit(&p_compile_function_declaration->v_parameters);
            ketl_free(p_state->p_allocator, p_compile_function_declaration->p_opcodes);
            ketl_namespace_node* p_node = ketl_namespace_find_by_index(
                p_compile_function_declaration->p_namespace, p_compile_function_declaration->namespace_node_index);
            p_node->variable.cfunc = ketl_dynamic_library_load_function(p_library_filename, 
                ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_node->s_name));
        }
    }
    
    compile_function_declarations_t_deinit(&compile_function_declarations);

    ketl_free(p_state->p_allocator, p_opcodes);
    uint64_t(*func)(void) = (uint64_t(*)(void))ketl_dynamic_library_load_function(p_library_filename, p_func_name);

    output_variable.uint64 = func();

    ketl_namespace_deinit(&local_namespace);

    return ketl_value_from_variable(output_variable, p_state->p_allocator);
}

void ketl_state_postload(ketl_state* p_state, ketl_lexer_t* p_lexer, ketl_namespace* p_namespace, compile_function_declarations_t* p_compile_function_declarations) {
    for (uint32_t i = 0; i < p_compile_function_declarations->size; ++i) {
        compile_function_declaration_t* p_compile_function_declaration = &p_compile_function_declarations->p_data[i];

        ketl_namespace local_namespace;
        ketl_namespace_node* p_node = ketl_namespace_find_by_index(
            p_compile_function_declaration->p_namespace,
            p_compile_function_declaration->namespace_node_index);
        ketl_namespace_init(&local_namespace, p_node->s_name, &p_state->atomic_strings, p_namespace, p_state->p_allocator);

        ketl_type* p_return_type = ((ketl_type_function*)p_node->variable.p_type)->p_type_signature->a_parameters[0].p_type;

        ketl_named_variable_type_info_t* p_function_parameters_named = p_compile_function_declaration->v_parameters.p_data;
        uint32_t parameters_count = p_compile_function_declaration->v_parameters.size;

        p_lexer->token_iterator = p_compile_function_declaration->start_pos;

        ketl_variable output_variable;
        uint32_t opcodes_size = 0u;
        uint8_t* p_opcodes = ketl_state_compile_function(p_state, p_lexer, p_compile_function_declaration->end_pos, &local_namespace, &opcodes_size, p_function_parameters_named, parameters_count, p_return_type, i, false, &output_variable);

        p_compile_function_declaration->p_opcodes = p_opcodes;
        p_compile_function_declaration->opcodes_size = opcodes_size;

        ketl_namespace_deinit(&local_namespace);
    }
}

bool ketl_state_load_module(ketl_state* p_state, const char* p_module_name, uint32_t length) {
    ketl_atomic_string s_module_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_module_name, length);
    return ketl_state_load_module_impl(p_state, s_module_name, NULL, NULL, false) != NULL;
}

ketl_namespace* ketl_state_load_module_impl(ketl_state* p_state, ketl_atomic_string s_module_name, const char* p_folder_path, ketl_namespace* p_namespace, bool export) {
    const char* p_module_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, s_module_name);

    char a_module_filename[256] = {0};
    snprintf(a_module_filename, ANN_ARRAY_SIZE(a_module_filename), "%s.ktl", p_module_name);

    uint64_t module_size = p_state->modules.size;
    ketl_modules_t_bucket* p_module_bucket = ketl_modules_t_get_or_insert_copy(&p_state->modules, s_module_name, (ketl_module_t){0});
    // old insert
    if (module_size == p_state->modules.size) {
        ketl_module_preload(&p_module_bucket->value, a_module_filename, p_folder_path, p_namespace, export, p_state);
        return &p_module_bucket->value.namespace;
    }

    bool top_loading = !p_state->loading_modules;
    p_state->loading_modules = true;

    ketl_module_init(&p_module_bucket->value, s_module_name, p_state);

    // preloading
    if (!ketl_module_preload(&p_module_bucket->value, a_module_filename, p_folder_path, p_namespace, export, p_state)) {
        ketl_modules_t_erase(&p_state->modules, p_module_bucket);
        return NULL;
    }

    // p_module_bucket could be invalid
    p_module_bucket = ketl_modules_t_get_or_insert_copy(&p_state->modules, s_module_name, (ketl_module_t){0});
    if (!top_loading) {
        return &p_module_bucket->value.namespace;
    }

    // postloading evetything
    KETL_HASH_MAP_FOREACH(ketl_modules_t, &p_state->modules,
        ketl_module_t* p_module = &__p_bucket->value;
        if (p_module->header_loaded && !p_module->body_loaded) {
            ketl_module_load(p_module, p_state);
        }
    );

    p_state->loading_modules = false;

    return &p_module_bucket->value.namespace;
}

static bool variable_is_namespace(ketl_variable* p_variable) {
    return p_variable->kind == KETL_VARIABLE_NAMESPACE;
}

static bool variable_is_type(ketl_variable* p_variable) {
    return p_variable->kind == KETL_VARIABLE_TYPE;
}

static bool variable_is_class(ketl_variable* p_variable) {
    return variable_is_type(p_variable) && ((ketl_type*)p_variable->p_pointer)->kind == KETL_TYPE_CLASS;
}

static bool variable_is_function(ketl_variable* p_variable) {
    return p_variable->kind == KETL_VARIABLE_CFUNC || p_variable->kind == KETL_VARIABLE_FUNC;
}

static bool variable_is_var(ketl_variable* p_variable) {
    return !variable_is_namespace(p_variable) && !variable_is_type(p_variable) && !variable_is_function(p_variable);
}

static bool namespace_has_vars(ketl_namespace* p_namespace) {
    for (uint32_t i = 0; i < p_namespace->v_nodes.size; ++i) {
        if (p_namespace->v_nodes.p_data[i].info.imported) {
            continue;
        }
        ketl_variable* p_variable = &p_namespace->v_nodes.p_data[i].variable;
        if (variable_is_var(p_variable)) {
            return true;
        }
    }
    return false;
}

static bool namespace_has_classes(ketl_namespace* p_namespace) {
    for (uint32_t i = 0; i < p_namespace->v_nodes.size; ++i) {
        if (p_namespace->v_nodes.p_data[i].info.imported) {
            continue;
        }
        ketl_variable* p_variable = &p_namespace->v_nodes.p_data[i].variable;
        if (variable_is_class(p_variable)) {
            return true;
        }
    }
    return false;
}

ANN_DEFINE(hir_const) {
    ketl_hir_const_info_t* p_consts_infos;
    uint8_t* p_consts;
    ketl_hir_const_index_t consts_count;
    uint32_t consts_size;
};

KETL_VECTOR_DECLARATION(hir_consts, hir_const)
KETL_VECTOR_DEFINITION(hir_consts, hir_const)

static bool consts_non_empty(hir_consts* p_consts) {
    for (uint32_t i = 0; i < p_consts->size; ++i) {
        if (p_consts->p_data[i].consts_count > 0) {
            return true;
        }
    }

    return false;
}

static void printout_read_only_data_namespace(ketl_namespace* p_namespace, ketl_state* p_state) {
    char a_size_name_buffer[16];

    for (uint32_t i = 0; i < p_namespace->v_nodes.size; ++i) {
        ketl_namespace_node* p_node = &p_namespace->v_nodes.p_data[i];
        if (p_node->info.imported || !variable_is_class(&p_node->variable)) {
            continue;
        }
        
        const char* p_variable_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_node->s_name);

        printf("    .globl    %s                    # @%s\n", p_variable_name, p_variable_name);
        printf("    .p2align  %d, 0x0\n", ketl_align_find(sizeof(void*)));
        printf("%s:\n", p_variable_name);

        ketl_type_class* p_class_type = p_node->variable.p_pointer;

        ketl_asm_x86_format_directive_size(sizeof(p_class_type->kind), a_size_name_buffer, ANN_ARRAY_SIZE(a_size_name_buffer));
        printf("    .%s   %d    # kind\n", a_size_name_buffer, p_class_type->kind);
        ketl_asm_x86_format_directive_size(sizeof(p_class_type->align_enum), a_size_name_buffer, ANN_ARRAY_SIZE(a_size_name_buffer));
        printf("    .%s   %d    # align_enum\n", a_size_name_buffer, p_class_type->align_enum);
        ketl_asm_x86_format_directive_size(sizeof(p_class_type->size), a_size_name_buffer, ANN_ARRAY_SIZE(a_size_name_buffer));
        printf("    .%s   %d    # size\n", a_size_name_buffer, p_class_type->size);

        ketl_asm_x86_format_directive_size(sizeof(p_class_type->s_name), a_size_name_buffer, ANN_ARRAY_SIZE(a_size_name_buffer));
        printf("    .%s   %d    # s_name, TODO\n", a_size_name_buffer, 0); // TODO
        ketl_asm_x86_format_directive_size(sizeof(p_class_type->fields_count), a_size_name_buffer, ANN_ARRAY_SIZE(a_size_name_buffer));
        printf("    .%s   %d    # fields_count\n", a_size_name_buffer, p_class_type->fields_count);
        ketl_asm_x86_format_directive_size(sizeof(p_class_type->p_fields), a_size_name_buffer, ANN_ARRAY_SIZE(a_size_name_buffer));
        printf("    .%s   %d    # p_fields, TODO\n", a_size_name_buffer, 0); // TODO

        /* TODO cool idea, but must dublicate in files where class is used - hard to track
        for (uint32_t field_index = 0; field_index < p_class_type->fields_count; ++field_index) {
            printf("    .set %s.%s, %"PRIu16"\n", p_variable_name, 
                ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_class_type->p_fields[field_index].s_name),
                ketl_type_get_field_offset((ketl_type*)p_class_type, p_class_type->p_fields[field_index].s_name, p_state));
        }
        */

        printout_read_only_data_namespace(&p_class_type->namespace, p_state);
    }
}

void ketl_state_print_compile2asm(ketl_state* p_state, const char* p_filepath, uint32_t length) {
    size_t after_last_slash_index = length;
    while (after_last_slash_index != 0 && p_filepath[after_last_slash_index - 1] != '/' && p_filepath[after_last_slash_index - 1] != '\\') {
        --after_last_slash_index;
    }

    const char* p_module_name = p_filepath + after_last_slash_index;
    uint32_t module_name_length = length - after_last_slash_index - (sizeof(".ktl") - 1);


    ketl_atomic_string s_module_name = ketl_atomic_strings_get(&p_state->atomic_strings, p_module_name, module_name_length);

    uint64_t module_size = p_state->modules.size;
    ketl_modules_t_bucket* p_module_bucket = ketl_modules_t_get_or_insert_copy(&p_state->modules, s_module_name, (ketl_module_t){0});

    // old insert
    if (module_size == p_state->modules.size) {
        // not supported for now
        ANN_ASSERT(false);
    }
    
    p_state->loading_modules = true;

    ketl_module_t* p_module = &p_module_bucket->value;
    ketl_module_init(p_module, s_module_name, p_state);

    p_module->header_loaded = true;

    ketl_atomic_string stashed_module_name = p_state->active_module_name;
    p_state->active_module_name = p_module->s_name;

    ANN_ASSERT(stashed_module_name == KETL_ATOMIC_STRING_EMPTY);
    uint32_t path_length = after_last_slash_index;
    p_module->p_path = ketl_alloc(p_state->p_allocator, path_length + 1);
    ketl_memcpy(p_module->p_path, p_filepath, path_length);
    p_module->p_path[path_length] = '\0';

    //////////////////////////////////////////

    FILE *p_module_file = fopen(p_filepath, "rb");
    ANN_ASSERT(p_module_file != NULL);
    
    fseek(p_module_file, 0L, SEEK_END);
    int64_t filesize = ftell(p_module_file);
    ANN_ASSERT(filesize >= 0);

    p_module->p_source = ketl_alloc(p_state->p_allocator, (uint64_t)filesize);

    fseek(p_module_file, 0L, SEEK_SET);
    fread(p_module->p_source, sizeof(*p_module->p_source), filesize, p_module_file);
    ANN_ASSERT(ferror(p_module_file) == 0);

    fclose(p_module_file);

    //////////////////////////////////////////////

    printf("    .def    @feat.00;\n");
    printf("    .scl    3;\n");
    printf("    .type   0;\n");
    printf("    .endef\n");
    printf("    .globl  @feat.00\n");
    printf("@feat.00 = 0\n");
    printf("    .intel_syntax noprefix\n");
    printf("    .file   \"%s\"\n", p_filepath + after_last_slash_index);

    ///////////////////////////////////////////

    uint32_t error_stream_mark = p_state->error_stream.size;

    ///////////////////////////////////////////
    
    ketl_lexer_init(&p_module->lexer, p_state->p_allocator);
    ketl_lexer_build_tokens(&p_module->lexer, ketl_atomic_strings_get(&p_state->atomic_strings, p_filepath, length), p_module->p_source, filesize);

    ///////////////////////////////////////////

    hir_consts consts;
    hir_consts_init(&consts, 4, p_state->p_allocator);

    uint32_t compile_function_mark = p_state->compile_function_declarations.size;

    {

        ketl_hir_t hir;
        ketl_parser_build_hir(p_state, &hir, &p_module->lexer, p_module->lexer.tokens.size, &p_module->namespace, NULL, 0, NULL, 0, true, p_state->p_allocator);


        if (p_state->error_stream.size > error_stream_mark) {
            fprintf(stderr, "%.*s", p_state->error_stream.size - error_stream_mark, p_state->error_stream.p_data + error_stream_mark);
            p_state->error_stream.size = error_stream_mark;
            return;
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
        ketl_asm_x86_build(&hir, &asm_builder, &asm_x86, 0);
        ketl_hir_deinit(&hir);

        if (true) {
            char a_buffer[256];
            snprintf(a_buffer, ANN_ARRAY_SIZE(a_buffer), "%.*s..init", module_name_length, p_module_name);

            printf("    .text                    # -- Functions\n");
            printf("    .def    %s;                    # @%s\n", a_buffer, a_buffer);
            printf("    .scl    2;\n");
            printf("    .type   32;\n");
            printf("    .endef\n");
            printf("    .globl	%s\n", a_buffer);
            printf("    .p2align	4\n");
            printf("%s:\n", a_buffer);
            printf(".seh_proc %s\n", a_buffer);

            char arr_buffer[4096];
            uint32_t length = ketl_asm_x86_format(p_state, &asm_x86, arr_buffer, ANN_ARRAY_SIZE(arr_buffer), false);
            printf("%.*s", length, arr_buffer);

            printf("    .seh_endproc\n");
        }
        ketl_asm_x86_builder_deinit(&asm_builder);

        /////////////////////////
        
        //uint32_t opcodes_size = 0u;
        //uint8_t* p_opcodes = ketl_asm_x86_compile(&asm_x86, &opcodes_size, p_state->p_allocator);
        ketl_asm_x86_deinit(&asm_x86);

        /////////////////////////

        //p_compile_function_declaration->p_opcodes = p_opcodes;
        //p_compile_function_declaration->opcodes_size = opcodes_size;
    }

    /////////////////////////////////////////////

    for (uint32_t i = compile_function_mark; i < p_state->compile_function_declarations.size; ++i) {
        compile_function_declarations_t_push_back_ref(&p_module->compile_function_declarations, &p_state->compile_function_declarations.p_data[i]);
    }

    p_state->compile_function_declarations.size = compile_function_mark;

    ///////////////////////////////////////////////

    for (uint32_t i = 0; i < p_module->compile_function_declarations.size; ++i) {
        compile_function_declaration_t* p_compile_function_declaration = &p_module->compile_function_declarations.p_data[i];
        ketl_namespace_node* p_node = ketl_namespace_find_by_index(
            p_compile_function_declaration->p_namespace,
            p_compile_function_declaration->namespace_node_index);

        ketl_namespace local_namespace;
        ketl_namespace_init(&local_namespace, p_node->s_name, &p_state->atomic_strings, &p_module->namespace, p_state->p_allocator);

        ketl_named_variable_type_info_t* p_function_parameters_named = p_compile_function_declaration->v_parameters.p_data;
        uint32_t parameters_count = p_compile_function_declaration->v_parameters.size;

        p_module->lexer.token_iterator = p_compile_function_declaration->start_pos;

        uint32_t error_stream_mark = p_state->error_stream.size;

        //////////////////////////////////

        ketl_type* p_return_type = ((ketl_type_function*)p_node->variable.p_type)->p_type_signature->a_parameters[0].p_type;
    
        ketl_hir_t hir;
        ketl_parser_build_hir(p_state, &hir, &p_module->lexer, p_compile_function_declaration->end_pos, 
            p_compile_function_declaration->p_namespace, p_function_parameters_named, parameters_count, p_return_type, i + 1, false, p_state->p_allocator);
        
        if (p_state->error_stream.size > error_stream_mark) {
            fprintf(stderr, "%.*s", p_state->error_stream.size - error_stream_mark, p_state->error_stream.p_data + error_stream_mark);
            p_state->error_stream.size = error_stream_mark;
            continue;
        }
        
    #if ANN_BUILD_DEBUG
        for (uint32_t j = 0u; j < hir.vars_count; ++j) {
            if (hir.p_vars[j].type == KETL_HIR_USED_TYPE_UNKNOWN) {
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
        ketl_asm_x86_build(&hir, &asm_builder, &asm_x86, i + 1);

        hir_consts_push_back_copy(&consts, (hir_const){
            .p_consts_infos = hir.p_consts_infos,
            .p_consts = hir.p_consts,
            .consts_count = hir.consts_count,
            .consts_size = hir.consts_size,
        });
        hir.p_consts_infos = NULL;
        hir.p_consts = NULL;
        ketl_hir_deinit(&hir);

        if (true) {
            const char* p_func_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_node->s_name);

            printf("    .def    %s;                    # @%s\n", p_func_name, p_func_name);
            printf("    .scl    2;\n");
            printf("    .type   32;\n");
            printf("    .endef\n");
            printf("    .globl	%s\n", p_func_name);
            printf("    .p2align	4\n");
            printf("%s:\n", p_func_name);
            printf(".seh_proc %s\n", p_func_name);

            char arr_buffer[131072];
            uint32_t length = ketl_asm_x86_format(p_state, &asm_x86, arr_buffer, ANN_ARRAY_SIZE(arr_buffer), false);
            printf("%.*s", length, arr_buffer);

            printf("    .seh_endproc\n");
        }
        ketl_asm_x86_builder_deinit(&asm_builder);

        /////////////////////////
        
        //uint32_t opcodes_size = 0u;
        //uint8_t* p_opcodes = ketl_asm_x86_compile(&asm_x86, &opcodes_size, p_state->p_allocator);
        ketl_asm_x86_deinit(&asm_x86);

        /////////////////////////

        //p_compile_function_declaration->p_opcodes = p_opcodes;
        //p_compile_function_declaration->opcodes_size = opcodes_size;

        ketl_namespace_deinit(&local_namespace);
    }

    ///////////////////////////////

    char a_size_name_buffer[16];

    if (namespace_has_vars(&p_module->namespace)) {
        printf("    .bss                    # -- Zero-initialized Variables\n");
    }

    for (uint32_t i = 0; i < p_module->namespace.v_nodes.size; ++i) {
        ketl_namespace_node* p_node = &p_module->namespace.v_nodes.p_data[i];
        if (p_node->info.imported || !variable_is_var(&p_node->variable)) {
            continue;
        }
        
        const char* p_variable_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_node->s_name);

        printf("    .globl    %s                    # @%s\n", p_variable_name, p_variable_name);
        printf("    .p2align  %d, 0x0\n", p_node->variable.p_type->align_enum);
        printf("%s:\n", p_variable_name);
        ketl_asm_x86_format_directive_size(ketl_type_get_stack_size(p_node->variable.p_type), a_size_name_buffer, ANN_ARRAY_SIZE(a_size_name_buffer));
        printf("    .%s   0\n", a_size_name_buffer);
    }

    ///////////////////////////////

    if (namespace_has_classes(&p_module->namespace) || consts_non_empty(&consts)) {
        printf("    .section   rdata,\"dr\"                    # -- Read-only Variables\n");
    }

    for (uint32_t j = 0; j < consts.size; ++j) {
        for (uint32_t i = 0; i < consts.p_data[j].consts_count; ++i) {
            ketl_hir_const_info_t* p_const_info = &consts.p_data[j].p_consts_infos[i];
            const char* p_variable_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_const_info->s_name);

            printf("%s:\n", p_variable_name);

            ANN_ASSERT(p_const_info->is_string);
            printf("    .asciz    \"%.*s\"\n", p_const_info->const_size, (const char*)consts.p_data[j].p_consts + p_const_info->const_offset);
        }
        
        ketl_free(p_state->p_allocator, consts.p_data[j].p_consts_infos);
        ketl_free(p_state->p_allocator, consts.p_data[j].p_consts);
    }
    
    hir_consts_deinit(&consts);

    printout_read_only_data_namespace(&p_module->namespace, p_state);

    {
        char a_buffer[256];
        snprintf(a_buffer, ANN_ARRAY_SIZE(a_buffer), "%.*s..init", module_name_length, p_module_name);
        
        printf("    .p2align    2, 0x0\n");
        printf("    .addrsig\n");
        printf("    .addrsig_sym %s\n", a_buffer);
    }

    for (uint32_t i = 0; i < p_module->compile_function_declarations.size; ++i) {
        compile_function_declaration_t* p_compile_function_declaration = &p_module->compile_function_declarations.p_data[i];
        ketl_namespace_node* p_node = ketl_namespace_find_by_index(
            p_compile_function_declaration->p_namespace,
            p_compile_function_declaration->namespace_node_index);
        
        const char* p_func_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_node->s_name);

        printf("    .addrsig_sym %s\n", p_func_name);
    }

    p_state->active_module_name = stashed_module_name;
    p_state->loading_modules = false;
}
