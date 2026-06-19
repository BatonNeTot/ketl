//🫖ketl
#include "namespace.h"

#include <stdio.h>

KETL_VECTOR_DEFINITION(namespace_nodes, ketl_namespace_node)
KETL_HASH_MAP_DEFINITION(namespace_map, ketl_atomic_string, uint32_t, ANN_HASH, ANN_EQUAL)
KETL_VECTOR_DEFINITION(namespace_parents, ketl_namespace*)

void ketl_namespace_init(ketl_namespace* p_namespace, ketl_atomic_string s_name, ketl_atomic_strings* p_atomic_strings, ketl_namespace* p_parent, const ketl_allocator* p_allocator) {
    namespace_nodes_init(&p_namespace->v_nodes, 16, p_allocator);
    namespace_map_init(&p_namespace->m_vars, p_allocator);
    namespace_parents_init(&p_namespace->v_parents, 4, p_allocator);
    p_namespace->s_name = s_name;

    if (p_parent == NULL) {
        p_namespace->s_fullname = s_name;
        return;
    }
    
    ketl_namespace_add_parent(p_namespace, p_parent);
    
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
    namespace_parents_deinit(&p_namespace->v_parents);
    namespace_map_deinit(&p_namespace->m_vars);
    namespace_nodes_deinit(&p_namespace->v_nodes);
}

void ketl_namespace_add_parent(ketl_namespace* p_namespace, ketl_namespace* p_parent) {
    namespace_parents_push_back_copy(&p_namespace->v_parents, p_parent);
}

bool ketl_namespace_is_empty(ketl_namespace* p_namespace) {
    return p_namespace->m_vars.size == 0 && p_namespace->v_nodes.size == 0;
}

// going up until module namepsace is met and stop
// module defined as a namespace direct chil of global
// which does not have a parent
static uint32_t namespace_print_fullname(ketl_namespace* p_namespace, char* p_buffer, uint32_t buffer_size, ketl_atomic_strings* p_atomic_strings) {
    if (p_namespace->v_parents.size == 0) {
        return 0;
    }

    ketl_namespace* p_parent = p_namespace->v_parents.p_data[0];
    ANN_ASSERT(p_parent);

    uint32_t printed = namespace_print_fullname(p_parent, p_buffer, buffer_size, p_atomic_strings);
    printed += snprintf(p_buffer + printed, buffer_size - printed,  ".%s", 
                ketl_atomic_strings_get_pointer(p_atomic_strings, p_namespace->s_name));

    return printed;
}

ketl_namespace_node* ketl_namespace_put(ketl_namespace* p_namespace, ketl_atomic_string s_key, ketl_atomic_string s_name, ketl_variable variable, ketl_namespace_node_info info, ketl_atomic_strings* p_atomic_strings, bool force) {
    if (s_name == KETL_ATOMIC_STRING_EMPTY) {
        s_name = s_key;
        if (p_namespace->v_parents.size > 0) {
            ANN_ASSERT(p_namespace->s_name != KETL_ATOMIC_STRING_EMPTY);
            
            char a_buffer[256] = {'\0'};

            uint32_t printed = namespace_print_fullname(p_namespace, a_buffer, ANN_ARRAY_SIZE(a_buffer), p_atomic_strings);
            printed += snprintf(a_buffer + printed, ANN_ARRAY_SIZE(a_buffer) - printed,  ".%s", 
                        ketl_atomic_strings_get_pointer(p_atomic_strings, s_name));

            if (info.export) {
                // skip first dot
                s_name = ketl_atomic_strings_get(p_atomic_strings, a_buffer + 1, printed - 1);
            } else {
                s_name = ketl_atomic_strings_get(p_atomic_strings, a_buffer, printed);
            }
        }
    }

    ketl_namespace_node new_node = {
        .variable = variable,
        .s_name = s_name,
        .s_key = s_key,
        .info = info,
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

    return &p_namespace->v_nodes.p_data[p_bucket->value];
}

ketl_namespace_node* ketl_namespace_find(ketl_namespace* p_namespace, ketl_atomic_string s_key) {
    namespace_map_bucket* p_bucket = namespace_map_get_or_null(&p_namespace->m_vars, s_key);
    if (p_bucket != NULL) {
        return p_namespace->v_nodes.p_data + p_bucket->value;
    }

    for (uint32_t i = 0; i < p_namespace->v_parents.size; ++i)  {
        ketl_namespace_node* p_node = ketl_namespace_find(p_namespace->v_parents.p_data[i], s_key);
        if (p_node != NULL) {
            return p_node;
        }
    }

    return NULL;
}

ketl_namespace_node* ketl_namespace_find_by_index(ketl_namespace* p_namespace, uint32_t index) {
    if (index >= p_namespace->v_nodes.size) {
        return NULL;
    }

    return &p_namespace->v_nodes.p_data[index];
}

uint32_t ketl_namespace_get_index(ketl_namespace* p_namespace, ketl_namespace_node* p_node) {
    if (p_node == NULL) {
        return 0;
    }

    ANN_ASSERT(p_namespace->v_nodes.p_data <= p_node && p_node < p_namespace->v_nodes.p_data + p_namespace->v_nodes.size);
    return p_node - p_namespace->v_nodes.p_data;
}

ketl_namespace* ketl_namespace_find_direct_parent(ketl_namespace* p_namespace, ketl_namespace_node* p_node) {
    if (p_node == NULL) {
        return NULL;
    }

    if (p_namespace->v_nodes.p_data <= p_node && p_node < p_namespace->v_nodes.p_data + p_namespace->v_nodes.size) {
        return p_namespace;
    }

    for (uint32_t i = 0; i < p_namespace->v_parents.size; ++i)  {
        ketl_namespace* p_parent = ketl_namespace_find_direct_parent(p_namespace->v_parents.p_data[i], p_node);
        if (p_parent != NULL) {
            return p_parent;
        }
    }

    return NULL;
}
