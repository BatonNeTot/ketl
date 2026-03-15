//🫖ketl
#ifndef ketl_variable_h
#define ketl_variable_h

#include "ketl/type.h"

#include "atomic_strings.h"
#include "func_impl.h"

#include "ketl/utils.h"

typedef uint8_t ketl_variable_kind;

enum {
    KETL_VARIABLE_NONE,

    KETL_VARIABLE_TYPE,
    KETL_VARIABLE_NAMESPACE,

    KETL_VARIABLE_POINTER,
    KETL_VARIABLE_CFUNC,
    KETL_VARIABLE_FUNC,
    KETL_VARIABLE_REFERENCE,

    KETL_VARIABLE_BOOL,

    KETL_VARIABLE_INT8,
    KETL_VARIABLE_INT16,
    KETL_VARIABLE_INT32,
    KETL_VARIABLE_INT64,

    KETL_VARIABLE_UINT8,
    KETL_VARIABLE_UINT16,
    KETL_VARIABLE_UINT32,
    KETL_VARIABLE_UINT64,

    KETL_VARIABLE_FLOAT32,
    KETL_VARIABLE_FLOAT64,
};


ANN_DEFINE(ketl_variable) {
	union {
		uint64_t stack;
		void* p_pointer;
		void (*cfunc)(void);
		ketl_function_header* p_func;

		bool boolean;

		int8_t int8;
		int16_t int16;
		int32_t int32;
		int64_t int64;
		
		uint8_t uint8;
		uint16_t uint16;
		uint32_t uint32;
		uint64_t uint64;
		
		float float32;
		double float64;
	};
	ketl_variable_kind kind;
	ketl_type* p_type;
};

void ketl_variable_set_type(ketl_variable* p_variable, ketl_type* p_type);

ketl_variable_kind ketl_variable_get_kind(ketl_type* p_type);

#endif // ketl_variable_h
