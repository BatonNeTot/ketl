//🫖ketl
#include "namespace.h"

KETL_VECTOR_DEFINITION(namespace_nodes, ketl_namespace_node)
KETL_HASH_MAP_DEFINITION(namespace_map, ketl_atomic_string, uint32_t, ANN_HASH, ANN_EQUAL)

void ketl_namespace_init(ketl_namespace* p_namespace, const ketl_allocator* p_allocator) {
    namespace_nodes_init(&p_namespace->v_nodes, 16, p_allocator);
    namespace_map_init(&p_namespace->m_vars, p_allocator);
}

void ketl_namespace_deinit(ketl_namespace* p_namespace) {
    namespace_map_deinit(&p_namespace->m_vars);
    namespace_nodes_deinit(&p_namespace->v_nodes);
}

//void ketl_namespace_copy(ketl_namespace* p_dst_namespace, ketl_namespace* p_src_namespace);

void ketl_namespace_put(ketl_namespace* p_namespace, ketl_atomic_string s_key, ketl_variable variable) {
    // TODO insert into vector uninitialized or something
    ketl_namespace_node new_node = {
        .variable = variable
    };
    uint32_t new_node_offset = p_namespace->v_nodes.size;
    namespace_nodes_push_back_ref(&p_namespace->v_nodes, &new_node);

    namespace_map_bucket* p_bucket = namespace_map_get_or_insert_copy(&p_namespace->m_vars, s_key, new_node_offset);
    if (p_bucket->value != new_node_offset) {
        // TODO check const stuff
        // replace if replacement possible
        // do error if not
        ANN_ASSERT(false);
    }
}

ketl_namespace_node* ketl_namespace_find(ketl_namespace* p_namespace, ketl_atomic_string s_key) {
    namespace_map_bucket* p_bucket = namespace_map_get_or_null(&p_namespace->m_vars, s_key);
    if (p_bucket == NULL) {
        return NULL;
    }

    return p_namespace->v_nodes.p_data + p_bucket->value;
}
