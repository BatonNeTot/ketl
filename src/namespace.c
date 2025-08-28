//🫖ketl
#include "namespace.h"

KETL_VECTOR_DEFINITION(namespace_nodes, ketl_namespace_node)
KETL_HASH_MAP_DEFINITION(namespace_map, ketl_atomic_string, uint32_t, ANN_HASH, ANN_EQUAL)

void ketl_namespace_init(ketl_namespace* pNamespace, const ketl_allocator* p_allocator) {
    namespace_nodes_init(&pNamespace->vNodes, 16, p_allocator);
    namespace_map_init(&pNamespace->mVars, p_allocator);
}

void ketl_namespace_deinit(ketl_namespace* pNamespace) {
    namespace_map_deinit(&pNamespace->mVars);
    namespace_nodes_deinit(&pNamespace->vNodes);
}

//void ketl_namespace_copy(ketl_namespace* pDstNamespace, ketl_namespace* pSrcNamespace);

void ketl_namespace_put(ketl_namespace* pNamespace, ketl_atomic_string sKey, ketl_variable variable) {
    // TODO insert into vector uninitialized or something
    ketl_namespace_node newNode = {
        .variable = variable
    };
    uint32_t newNodeOffset = pNamespace->vNodes.size;
    namespace_nodes_push_back_ref(&pNamespace->vNodes, &newNode);

    namespace_map_bucket* pBucket = namespace_map_get_or_insert_copy(&pNamespace->mVars, sKey, newNodeOffset);
    if (pBucket->value != newNodeOffset) {
        // TODO check const stuff
        // replace if replacement possible
        // do error if not
        ANN_ASSERT(false);
    }
}

ketl_namespace_node* ketl_namespace_find(ketl_namespace* pNamespace, ketl_atomic_string sKey) {
    namespace_map_bucket* pBucket = namespace_map_get_or_null(&pNamespace->mVars, sKey);
    if (pBucket == NULL) {
        return NULL;
    }

    return pNamespace->vNodes.p_data + pBucket->value;
}
