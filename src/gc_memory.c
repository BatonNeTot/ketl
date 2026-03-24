//🫖ketl
#include "gc_memory.h"

#include "type_impl.h"

KETL_VECTOR_DEFINITION(objects, void*)
KETL_TREE_MAP_DEFINITION(object_info_map, void*, ketl_gc_info, ANN_LESS)

void ketl_gc_init(ketl_gc* p_gc, const ketl_allocator *p_allocator) {
    *p_gc = (ketl_gc){0};
    p_gc->p_allocator = p_allocator;
    objects_init(&p_gc->root_objects, 16, p_allocator);
    objects_init(&p_gc->collect_buffer, 16, p_allocator);
    object_info_map_init(&p_gc->object_info, 16, p_allocator);
    p_gc->flag_usage = false;
}

void ketl_gc_deinit(ketl_gc* p_gc) {
    object_info_map_deinit(&p_gc->object_info);
    objects_deinit(&p_gc->collect_buffer);
    objects_deinit(&p_gc->root_objects);
}

void* ketl_gc_create(ketl_gc* p_gc, ketl_type* p_type, uint8_t flags) {
    uint64_t obj_size = ketl_type_get_size(p_type);
    void* p_object = ketl_alloc(p_gc->p_allocator, obj_size);
    ketl_gc_reg(p_gc, p_object, p_type, 1, flags | KETL_GC_FREE_AFTER_USE);
    return p_object;
}

void* ketl_gc_create_array_of_type(ketl_gc* p_gc, ketl_type* p_type, uint64_t count, uint8_t flags) {
    uint64_t obj_size = ketl_type_get_stack_size(p_type);
    uint64_t mem_size = obj_size * count;
    ketl_array* p_array_obj = ketl_alloc(p_gc->p_allocator, sizeof(ketl_array));
    p_array_obj->size = count;
    p_array_obj->capacity = count;
    p_array_obj->p_data = ketl_alloc(p_gc->p_allocator, mem_size);
    ketl_memset(p_array_obj->p_data, 0, mem_size);
    ketl_gc_reg(p_gc, p_array_obj, p_type, count, flags | KETL_GC_FREE_AFTER_USE);
    return p_array_obj;
}

void ketl_gc_append_value(ketl_gc* p_gc, ketl_type* p_type, ketl_array* p_array, uint64_t value) {
    uint64_t value_size = ketl_type_get_stack_size(p_type);
    if (p_array->capacity == 0) {
        ANN_ASSERT(p_array->p_data == NULL);
        p_array->capacity = 4;
        p_array->p_data = ketl_alloc(p_gc->p_allocator, value_size * p_array->capacity);
    } else if (p_array->size >= p_array->capacity) {
        ANN_ASSERT(p_array->p_data != NULL);
        p_array->capacity += p_array->capacity / 2;
        p_array->p_data = ketl_realloc(p_gc->p_allocator, p_array->p_data, value_size * p_array->capacity);
    }
    ketl_memcpy(p_array->p_data + p_array->size++ * value_size, &value, value_size);
}

void ketl_gc_reg(ketl_gc* p_gc, void* p_object, ketl_type* p_type, uint64_t count, uint8_t flags) {
    ketl_gc_info info = {
        .p_type = p_type,
        .count = count,
        .flag_usage = p_gc->flag_usage,
        .free_after_use = flags & KETL_GC_FREE_AFTER_USE,
    };
    object_info_map_get_or_insert_ref(&p_gc->object_info, p_object, &info);
    if (flags & KETL_GC_ROOT) {
        objects_push_back_copy(&p_gc->root_objects, p_object);
    }
}

static void* find_object_start(ketl_gc* p_gc, const void* p_inside_object, ketl_gc_info** pp_info) {
    if (p_gc->object_info.size == 0) {
        return NULL;
    }

    object_info_map_node* p_nodes = p_gc->object_info.p_nodes;
    object_info_map_node* p_node = p_nodes + p_gc->object_info.root_offset;
    ANN_FOREVER {
        if (p_inside_object < p_node->key) {
            if (p_node->left_offset == (uint32_t)(-1)) {
                return NULL;
            }
            p_node = p_nodes + p_node->left_offset;
            continue;
        }
        if (p_node->key < p_inside_object) {
            if (p_node->right_offset == (uint32_t)(-1)) {
                // TODO check if it is in the range of a type and return p_node->key
                return NULL;
            }
            p_node = p_nodes + p_node->right_offset;
            continue;
        }
        *pp_info = &p_node->value;
        return p_node->key;
    }
}

uint64_t ketl_gc_get_allocation_count(ketl_gc* p_gc, void* p_object) {
    ketl_gc_info *p_info;
    if (find_object_start(p_gc, p_object, &p_info) == NULL) {
        return 0;
    }
    return p_info->count;
}

static void mark_objects(ketl_gc* p_gc) {
    bool flag_usage = (p_gc->flag_usage ^= true);
    objects_resize(&p_gc->collect_buffer, p_gc->root_objects.size);
    ketl_memcpy(p_gc->collect_buffer.p_data, p_gc->root_objects.p_data, p_gc->root_objects.size * sizeof(void*));
    while (p_gc->collect_buffer.size) {
        void* p_object = p_gc->root_objects.p_data[--p_gc->collect_buffer.size];
        ketl_gc_info* p_info;
        void* p_root = find_object_start(p_gc, p_object, &p_info);
        if (p_root && p_info->flag_usage != flag_usage) {
            p_info->flag_usage = flag_usage;
            objects_push_back_copy(&p_gc->root_objects, p_info->p_type);
            // TODO collect fields to collect_buffer
        }
    }
}

static uint32_t visit_node_and_swipe(ketl_gc* p_gc, uint32_t node_offset) {
    if (node_offset == (uint32_t)(-1)) {
        return (uint32_t)(-1);
    }

    object_info_map_node* p_nodes = p_gc->object_info.p_nodes;
    object_info_map_node* p_node = p_nodes + node_offset;
    p_node->left_offset = visit_node_and_swipe(p_gc, p_node->left_offset);
    p_node->right_offset = visit_node_and_swipe(p_gc, p_node->right_offset);

    if (p_node->value.flag_usage != p_gc->flag_usage) {
        // TODO place for destructor if needed
        if (p_node->value.free_after_use) {
            ketl_free(p_gc->p_allocator, p_node->key);
        }
        return (uint32_t)(-1);
    }

    p_node->height = 1 + ANN_MAX(__object_info_map_height(p_nodes, p_node->left_offset), __object_info_map_height(p_nodes, p_node->right_offset));

    return __object_info_map_balance(p_nodes, node_offset);
}

static void swipe_objects(ketl_gc* p_gc) {
    objects_clear(&p_gc->collect_buffer);
    p_gc->object_info.root_offset = visit_node_and_swipe(p_gc, p_gc->object_info.root_offset);
}

void ketl_gc_collect(ketl_gc* p_gc) {
    mark_objects(p_gc);
    swipe_objects(p_gc);
}
