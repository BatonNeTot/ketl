//🫖ketl
#ifndef ketl_namespace_h
#define ketl_namespace_h

#include "ketl/type.h"

#include "variable.h"
#include "atomic_strings.h"

#include "ketl/utils.h"

KETL_DEFINE(ketl_namespace_node) {
    ketl_variable variable;
};

KETL_VECTOR_DECLARATION(namespace_nodes, ketl_namespace_node)
KETL_HASH_MAP_DECLARATION(namespace_map, ketl_atomic_string, uint32_t)

KETL_DEFINE(ketl_namespace) {
    namespace_nodes vNodes;
    namespace_map mVars;
};

void ketl_namespace_init(ketl_namespace* pNamespace, const ketl_allocator* pAllocator);

void ketl_namespace_deinit(ketl_namespace* pNamespace);

void ketl_namespace_copy(ketl_namespace* pDstNamespace, ketl_namespace* pSrcNamespace);

void ketl_namespace_put(ketl_namespace* pNamespace, ketl_atomic_string sKey, ketl_variable variable);

ketl_namespace_node* ketl_namespace_find(ketl_namespace* pNamespace, ketl_atomic_string sKey);

#endif // ketl_namespace_h
