//🫖ketl
#ifndef ketl_module_h
#define ketl_module_h

#include "namespace.h"
#include "atomic_strings.h"

#include "ketl/utils.h"

ANN_FORWARD(ketl_state);


ANN_DEFINE(ketl_module_t) {
    ketl_atomic_string s_name;
    ketl_namespace namespace;
};

void ketl_module_init(ketl_module_t* p_module, ketl_atomic_string s_name, ketl_state* p_state);

bool ketl_module_load(ketl_module_t* p_module, ketl_namespace* p_namespace, ketl_state* p_state);

void ketl_module_deinit(ketl_module_t* p_module);

#endif
