//🫖ketl
#ifndef ketl_namespace_h
#define ketl_namespace_h

#include "ketl/type.h"

#include "atomic_strings.h"

#include "ketl/utils.h"

typedef uint8_t ketl_namespace_value_type;

enum __KETL_NAMESPACE_VALUE_TYPE {
    KETL_NAMESPACE_VALUE_TYPE,
    KETL_NAMESPACE_VALUE_VAR,
};

KETL_DEFINE(ketl_namespace_value) {
    ketl_namespace_value_type type;
    // TODO some properties to fill in the aligment gap?
    union {
        ketl_type* pType;
        struct {
            ketl_type* pType;
            void* pValue;
        } var;
    };
};

KETL_DEFINE(ketl_namespace_node) {
    uint32_t nextOffset;
    ketl_namespace_value value;
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

void ketl_namespace_put(ketl_namespace* pNamespace, ketl_atomic_string sKey, ketl_namespace_value value);

ketl_namespace_node* ketl_namespace_find(ketl_namespace* pNamespace, ketl_atomic_string sKey);

#endif // ketl_namespace_h
