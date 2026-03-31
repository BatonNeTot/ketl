//🫖ketl
#ifndef ketl_namespace_h
#define ketl_namespace_h

#include "ketl/type.h"

#include "variable.h"
#include "atomic_strings.h"

#include "ketl/utils.h"

ANN_DEFINE(ketl_namespace_node_info) {
    // TODO currently 'export' keyword does not affect accessibility of the symbol, absence only adds dot in the name at the beginning
    bool export;
    bool c_symbol;
    bool imported;
};

ANN_DEFINE(ketl_namespace_node) {
    ketl_variable variable;
    ketl_atomic_string s_name;
    ketl_atomic_string s_key;
    ketl_namespace_node_info info;
};

KETL_VECTOR_DECLARATION(namespace_nodes, ketl_namespace_node)
KETL_HASH_MAP_DECLARATION(namespace_map, ketl_atomic_string, uint32_t)

ANN_DEFINE(ketl_namespace) {
    namespace_nodes v_nodes;
    namespace_map m_vars;
    ketl_namespace* p_parent;
    ketl_atomic_string s_name;
    ketl_atomic_string s_fullname;
};

void ketl_namespace_init(ketl_namespace* p_namespace, ketl_atomic_string s_name, ketl_atomic_strings* p_atomic_strings, ketl_namespace* p_parent, const ketl_allocator* p_allocator);

void ketl_namespace_deinit(ketl_namespace* p_namespace);

bool ketl_namespace_is_empty(ketl_namespace* p_namespace);

void ketl_namespace_copy(ketl_namespace* p_dst_namespace, ketl_namespace* p_src_namespace);

ketl_namespace_node* ketl_namespace_put(ketl_namespace* p_namespace, ketl_atomic_string s_key, ketl_variable variable, ketl_namespace_node_info info, ketl_atomic_strings* p_atomic_strings, bool force);

ketl_namespace_node* ketl_namespace_find(ketl_namespace* p_namespace, ketl_atomic_string s_key);

ketl_namespace_node* ketl_namespace_find_by_index(ketl_namespace* p_namespace, uint32_t index);

uint32_t ketl_namespace_get_index(ketl_namespace* p_namespace, ketl_namespace_node* p_node);

ketl_namespace* ketl_namespace_find_direct_parent(ketl_namespace* p_namespace, ketl_namespace_node* p_node);

#endif // ketl_namespace_h
