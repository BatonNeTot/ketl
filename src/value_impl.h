//🫖ketl
#ifndef ketl_value_impl_h
#define ketl_value_impl_h

#include "ketl/value.h"

#include "variable.h"
#include "memory_impl.h"


// TODO quick solution
#define ketl_value_from_variable_pointer(p_variable) ((ketl_value*)(p_variable))

ketl_value* ketl_value_from_variable(ketl_variable variable, const ketl_allocator* p_allocator);

#endif
