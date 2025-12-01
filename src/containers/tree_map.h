//🫖ketl
#ifndef ketl_containers_tree_map_h
#define ketl_containers_tree_map_h

#include "ketl/utils.h"

#include "memory_impl.h"

#define KETL_TREE_MAP_DECLARATION(name, k_type, v_type)\
ANN_DEFINE(ANN_CONCAT(name,_node)) {\
    union {\
    uint32_t left_offset;\
    uint32_t next_offset;\
    };\
    uint32_t right_offset;\
	uint16_t height;\
	k_type key;\
    v_type value;\
};\
ANN_DEFINE(name) {\
    const ketl_allocator* p_allocator;\
	ANN_CONCAT(name,_node)* p_nodes;\
	uint32_t free_node_offset;\
	uint32_t root_offset;\
	uint32_t size;\
	uint32_t capacity;\
};\
void ANN_CONCAT(name,_init)(name* p_map, uint32_t initial_capacity, const ketl_allocator* p_allocator);\
void ANN_CONCAT(name,_deinit)(name* p_map);\
ANN_CONCAT(name,_node)* ANN_CONCAT(name,_get_or_insert_copy)(name* p_map, k_type key, v_type value);\
ANN_CONCAT(name,_node)* ANN_CONCAT(name,_get_or_insert_ref)(name* p_map, k_type key, v_type const* p_value);\
ANN_CONCAT(name,_node)* ANN_CONCAT(name,_erase)(name* p_map, k_type key);\

#define KETL_TREE_MAP_DEFINITION(name, k_type, v_type, k_less)\
void ANN_CONCAT(name,_init)(name* p_map, uint32_t initial_capacity, const ketl_allocator* p_allocator) {\
    const uint32_t array_size = sizeof(ANN_CONCAT(name,_node)) * initial_capacity;\
    \
    ANN_CONCAT(name,_node)* p_nodes = ketl_alloc(p_allocator, array_size);\
    \
    *p_map = (name){\
        .p_allocator = p_allocator,\
        .p_nodes = p_nodes,\
        .free_node_offset = 0,\
        .root_offset = (uint32_t)(-1),\
        .size = 0,\
        .capacity = initial_capacity};\
    \
    --initial_capacity;\
    for (uint32_t i = 0u; i < initial_capacity; ++i) {\
        p_nodes[i].next_offset = i + 1;\
    }\
    p_nodes[initial_capacity].next_offset = (uint32_t)(-1);\
}\
void ANN_CONCAT(name,_deinit)(name* p_map) {\
    ketl_free(p_map->p_allocator, p_map->p_nodes);\
}\
static uint16_t ANN_CONCAT(__,name,_height)(ANN_CONCAT(name,_node)* p_nodes, uint32_t node_offset) {\
    if (node_offset == (uint32_t)(-1)) {\
        return 0;\
    }\
    ANN_CONCAT(name,_node)* p_node = p_nodes + node_offset;\
    return p_node->height;\
}\
static int64_t ANN_CONCAT(__,name,_get_balance)(ANN_CONCAT(name,_node)* p_nodes, uint32_t node_offset) {\
    if (node_offset == (uint32_t)(-1)) {\
        return 0;\
    }\
    ANN_CONCAT(name,_node)* p_node = p_nodes + node_offset;\
    return ANN_CONCAT(__,name,_height)(p_nodes, p_node->right_offset) - ANN_CONCAT(__,name,_height)(p_nodes, p_node->left_offset);\
}\
static uint32_t ANN_CONCAT(__,name,_rotate_left)(ANN_CONCAT(name,_node)* p_nodes, uint32_t node_offset) {\
    ANN_CONCAT(name,_node) *p_x = p_nodes + node_offset;\
    uint32_t p_yoffset = p_x->right_offset;\
    ANN_CONCAT(name,_node) *p_y = p_nodes + p_yoffset;\
    uint32_t T2Offset = p_y->left_offset;\
\
    p_y->left_offset = node_offset;\
    p_x->right_offset = T2Offset;\
\
    p_x->height = 1 + ANN_MAX(ANN_CONCAT(__,name,_height)(p_nodes, p_x->left_offset), ANN_CONCAT(__,name,_height)(p_nodes, p_x->right_offset));\
    p_y->height = 1 + ANN_MAX(ANN_CONCAT(__,name,_height)(p_nodes, p_y->left_offset), ANN_CONCAT(__,name,_height)(p_nodes, p_y->right_offset));\
\
    return p_yoffset;\
}\
static uint32_t ANN_CONCAT(__,name,_rotate_right)(ANN_CONCAT(name,_node)* p_nodes, uint32_t node_offset) {\
    ANN_CONCAT(name,_node) *p_x = p_nodes + node_offset;\
    uint32_t p_yoffset = p_x->left_offset;\
    ANN_CONCAT(name,_node) *p_y = p_nodes + p_yoffset;\
    uint32_t T2Offset = p_y->right_offset;\
\
    p_y->right_offset = node_offset;\
    p_x->left_offset = T2Offset;\
\
    p_x->height = 1 + ANN_MAX(ANN_CONCAT(__,name,_height)(p_nodes, p_x->left_offset), ANN_CONCAT(__,name,_height)(p_nodes, p_x->right_offset));\
    p_y->height = 1 + ANN_MAX(ANN_CONCAT(__,name,_height)(p_nodes, p_y->left_offset), ANN_CONCAT(__,name,_height)(p_nodes, p_y->right_offset));\
\
    return p_yoffset;\
}\
static uint32_t ANN_CONCAT(__,name,_balance)(ANN_CONCAT(name,_node)* p_nodes, uint32_t node_offset) {\
    ANN_CONCAT(name,_node)* p_node = p_nodes + node_offset;\
    int32_t balance = (int32_t)ANN_CONCAT(__,name,_get_balance)(p_nodes, node_offset);\
    \
    if (balance < -1) {\
        if (ANN_CONCAT(__,name,_get_balance)(p_nodes, p_node->left_offset) <= 0) {\
            return ANN_CONCAT(__,name,_rotate_right)(p_nodes, node_offset);\
        } else {\
            p_node->left_offset = ANN_CONCAT(__,name,_rotate_left)(p_nodes, p_node->left_offset);\
            return ANN_CONCAT(__,name,_rotate_right)(p_nodes, node_offset);\
        }\
    }\
\
    if (balance > 1) {\
        if (ANN_CONCAT(__,name,_get_balance)(p_nodes, p_node->right_offset) >= 0) {\
            return ANN_CONCAT(__,name,_rotate_left)(p_nodes, node_offset);\
        } else {\
            p_node->right_offset = ANN_CONCAT(__,name,_rotate_right)(p_nodes, p_node->right_offset);\
            return ANN_CONCAT(__,name,_rotate_left)(p_nodes, node_offset);\
        }\
    }\
\
    return node_offset;\
}\
static uint32_t ANN_CONCAT(__,name,_get_or_insert_impl)(name* p_map, uint32_t node_offset, k_type key, uint32_t* p_found_offset) {\
    ANN_CONCAT(name,_node)* p_nodes = p_map->p_nodes;\
    if (node_offset == (uint32_t)(-1)) {\
        if (p_map->free_node_offset == (uint32_t)(-1)) {\
            /* TODO allocate more */\
        }\
\
        uint32_t new_node_offset = p_map->free_node_offset;\
        ANN_CONCAT(name,_node)* p_new_node = p_nodes + new_node_offset;\
        p_map->free_node_offset = p_new_node->next_offset;\
\
        p_new_node->left_offset = (uint32_t)(-1);\
        p_new_node->right_offset = (uint32_t)(-1);\
        p_new_node->height = 1;\
        p_new_node->key = key;\
\
        return *p_found_offset = new_node_offset;\
    }\
\
    ANN_CONCAT(name,_node)* p_node = p_nodes + node_offset;\
    if (k_less(key, p_node->key)) {\
        p_node->left_offset = ANN_CONCAT(__,name,_get_or_insert_impl)(p_map, p_node->left_offset, key, p_found_offset);\
    } else if (k_less(p_node->key, key)) {\
        p_node->right_offset = ANN_CONCAT(__,name,_get_or_insert_impl)(p_map, p_node->right_offset, key, p_found_offset);\
    } else {\
        return *p_found_offset = node_offset;\
    }\
\
    p_node->height = 1 + ANN_MAX(ANN_CONCAT(__,name,_height)(p_nodes, p_node->left_offset), ANN_CONCAT(__,name,_height)(p_nodes, p_node->right_offset));\
\
    return ANN_CONCAT(__,name,_balance)(p_nodes, node_offset);\
}\
ANN_CONCAT(name,_node)* ANN_CONCAT(name,_get_or_insert_copy)(name* p_map, k_type key, v_type value) {\
    uint32_t free_node_offset = p_map->free_node_offset;\
    uint32_t found_offset;\
    p_map->root_offset = ANN_CONCAT(__,name,_get_or_insert_impl)(p_map, p_map->root_offset, key, &found_offset);\
\
    ANN_CONCAT(name,_node)* p_found_node = p_map->p_nodes + found_offset;\
    if (free_node_offset == found_offset) {\
        p_found_node->value = value;\
    }\
    \
    return p_found_node;\
}\
ANN_CONCAT(name,_node)* ANN_CONCAT(name,_get_or_insert_ref)(name* p_map, k_type key, v_type const* p_value) {\
    uint32_t free_node_offset = p_map->free_node_offset;\
    uint32_t found_offset;\
    p_map->root_offset = ANN_CONCAT(__,name,_get_or_insert_impl)(p_map, p_map->root_offset, key, &found_offset);\
\
    ANN_CONCAT(name,_node)* p_found_node = p_map->p_nodes + found_offset;\
    if (free_node_offset == found_offset) {\
        p_found_node->value = *p_value;\
    }\
    \
    return p_found_node;\
}\
static uint64_t ANN_CONCAT(__,name,_erase_leftmost_impl)(ANN_CONCAT(name,_node)* p_nodes, uint32_t node_offset, uint32_t* p_found_offset) {\
    ANN_CONCAT(name,_node)* p_node = p_nodes + node_offset;\
    if (p_node->left_offset == (uint32_t)(-1)) {\
        *p_found_offset = node_offset;\
        return p_node->right_offset;\
    } else {\
        p_node->left_offset = (uint32_t)ANN_CONCAT(__,name,_erase_leftmost_impl)(p_nodes, p_node->left_offset, p_found_offset);\
    }\
\
    p_node->height = 1 + ANN_MAX(ANN_CONCAT(__,name,_height)(p_nodes, p_node->left_offset), ANN_CONCAT(__,name,_height)(p_nodes, p_node->right_offset));\
\
    return ANN_CONCAT(__,name,_balance)(p_nodes, node_offset);\
}\
static uint64_t ANN_CONCAT(__,name,_erase_impl)(ANN_CONCAT(name,_node)* p_nodes, uint32_t node_offset, k_type key, uint32_t* p_found_offset) {\
    if (node_offset == (uint32_t)(-1)) {\
        return node_offset;\
    }\
\
    ANN_CONCAT(name,_node)* p_node = p_nodes + node_offset;\
    if (k_less(key, p_node->key)) {\
        p_node->left_offset = (uint32_t)ANN_CONCAT(__,name,_erase_impl)(p_nodes, p_node->left_offset, key, p_found_offset);\
    } else if (k_less(p_node->key, key)) {\
        p_node->right_offset = (uint32_t)ANN_CONCAT(__,name,_erase_impl)(p_nodes, p_node->right_offset, key, p_found_offset);\
    } else {\
        if (p_node->left_offset == (uint32_t)(-1)) {\
            *p_found_offset = node_offset;\
            return p_node->right_offset;\
        } \
        if (p_node->right_offset == (uint32_t)(-1)) {\
            *p_found_offset = node_offset;\
            return p_node->left_offset;\
        }\
\
        uint32_t left_offset = p_node->left_offset;\
        uint32_t right_offset = (uint32_t)ANN_CONCAT(__,name,_erase_leftmost_impl)(p_nodes, p_node->right_offset, p_found_offset);\
\
        uint32_t replacement_offset = *p_found_offset;\
        *p_found_offset = node_offset;\
\
        node_offset = replacement_offset;\
        p_node = p_nodes + replacement_offset;\
\
        p_node->right_offset = right_offset;\
        p_node->left_offset = left_offset;\
    }\
\
    p_node->height = 1 + ANN_MAX(ANN_CONCAT(__,name,_height)(p_nodes, p_node->left_offset), ANN_CONCAT(__,name,_height)(p_nodes, p_node->right_offset));\
\
    return ANN_CONCAT(__,name,_balance)(p_nodes, node_offset);\
}\
ANN_CONCAT(name,_node)* ANN_CONCAT(name,_erase)(name* p_map, k_type key) {\
    uint32_t found_offset = (uint32_t)(-1);\
    ANN_CONCAT(name,_node)* p_nodes = p_map->p_nodes; \
    p_map->root_offset = (uint32_t)ANN_CONCAT(__,name,_erase_impl)(p_nodes, p_map->root_offset, key, &found_offset);\
    if (found_offset == (uint32_t)(-1)) {\
        return NULL;\
    }\
    \
    ANN_CONCAT(name,_node)* p_node = p_nodes + found_offset;\
    p_node->next_offset = p_map->free_node_offset;\
    p_map->free_node_offset = found_offset;\
    return p_node;\
}\

#endif // ketl_containers_tree_map_h
