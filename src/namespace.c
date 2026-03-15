//🫖ketl
#include "namespace.h"

KETL_VECTOR_DEFINITION(namespace_nodes, ketl_namespace_node)
KETL_HASH_MAP_DEFINITION(namespace_map, ketl_atomic_string, uint32_t, ANN_HASH, ANN_EQUAL)

void ketl_namespace_init(ketl_namespace* p_namespace, ketl_atomic_string s_name, ketl_atomic_strings* p_atomic_strings, ketl_namespace* p_parent, const ketl_allocator* p_allocator) {
    namespace_nodes_init(&p_namespace->v_nodes, 16, p_allocator);
    namespace_map_init(&p_namespace->m_vars, p_allocator);
    p_namespace->p_parent = p_parent;
    p_namespace->s_name = s_name;

    if (p_parent == NULL) {
        p_namespace->s_fullname = s_name;
        return;
    }
    
    const char* p_parent_fullname = ketl_atomic_strings_get_pointer(p_atomic_strings, p_parent->s_fullname);
    uint64_t parent_fullname_length = ketl_strlen(p_parent_fullname);

    const char* p_name = ketl_atomic_strings_get_pointer(p_atomic_strings, s_name);
    uint64_t name_length = ketl_strlen(p_name);

    uint64_t total_length = parent_fullname_length + name_length + 1;

    char a_buffer[256] = {0};
    ANN_ASSERT(total_length <= ANN_ARRAY_SIZE(a_buffer));
    ketl_memcpy(a_buffer, p_parent_fullname, parent_fullname_length);
    a_buffer[parent_fullname_length] = '.';
    ketl_memcpy(a_buffer + parent_fullname_length + 1, p_name, name_length);

    p_namespace->s_fullname = ketl_atomic_strings_get(p_atomic_strings, a_buffer, total_length);
}

void ketl_namespace_deinit(ketl_namespace* p_namespace) {
    namespace_map_deinit(&p_namespace->m_vars);
    namespace_nodes_deinit(&p_namespace->v_nodes);
}

bool ketl_namespace_is_empty(ketl_namespace* p_namespace) {
    return p_namespace->m_vars.size == 0 && p_namespace->v_nodes.size == 0;
}

//void ketl_namespace_copy(ketl_namespace* p_dst_namespace, ketl_namespace* p_src_namespace);

ketl_variable* ketl_namespace_put(ketl_namespace* p_namespace, ketl_atomic_string s_key, ketl_variable variable, bool force) {
    // TODO insert into vector uninitialized or something
    ketl_namespace_node new_node = {
        .variable = variable,
        .s_name = s_key,
    };
    uint32_t new_node_offset = p_namespace->v_nodes.size;

    namespace_map_bucket* p_bucket = namespace_map_get_or_insert_copy(&p_namespace->m_vars, s_key, new_node_offset);
    if (p_bucket->value != new_node_offset) {
        if (!force) {
            // TODO check const stuff
            // replace if replacement possible
            // do error if not
            ANN_ASSERT(false);
        }
        
        p_namespace->v_nodes.p_data[p_bucket->value] = new_node;
    } else {
        namespace_nodes_push_back_ref(&p_namespace->v_nodes, &new_node);
    }

    return &p_namespace->v_nodes.p_data[p_bucket->value].variable;
}

ketl_namespace_node* ketl_namespace_find(ketl_namespace* p_namespace, ketl_atomic_string s_key) {
    namespace_map_bucket* p_bucket = namespace_map_get_or_null(&p_namespace->m_vars, s_key);
    if (p_bucket != NULL) {
        return p_namespace->v_nodes.p_data + p_bucket->value;
    }

    if (p_namespace->p_parent != NULL) {
        return ketl_namespace_find(p_namespace->p_parent, s_key);
    }

    return NULL;
}
