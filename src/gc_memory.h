//🫖ketl
#ifndef ketl_gc_memory_h
#define ketl_gc_memory_h

#include "ketl/memory.h"
#include "ketl/type.h"

#include "containers/vector.h"
#include "containers/tree_map.h"

#include "ketl/utils.h"

ANN_DEFINE(ketl_gc_info) {
    ketl_type* p_type;
    uint64_t count;
    bool flag_usage;
    bool free_after_use;
};

KETL_VECTOR_DECLARATION(objects, void*)
KETL_TREE_MAP_DECLARATION(object_info_map, void*, ketl_gc_info)

ANN_DEFINE(ketl_gc) {
    const ketl_allocator *p_allocator;
    objects root_objects;
    objects collect_buffer;
    object_info_map object_info;
    bool flag_usage;
};

void ketl_gc_init(ketl_gc* p_gc, const ketl_allocator *p_allocator);

void ketl_gc_deinit(ketl_gc* p_gc);

#define KETL_GC_ROOT            0x01
#define KETL_GC_FREE_AFTER_USE  0x02

void* ketl_gc_create(ketl_gc* p_gc, ketl_type* p_type, uint8_t flags);

void* ketl_gc_create_array_of_type(ketl_gc* p_gc, ketl_type* p_type, uint64_t size, uint8_t* initial_data, uint64_t initial_data_mem_size, uint8_t flags);

ANN_FORWARD(ketl_array);

void* ketl_gc_create_slice_of_type(ketl_gc* p_gc, ketl_type* p_type, uint64_t size, ketl_array* mirrored_array, uint64_t mirrored_offset, uint8_t flags);

void ketl_gc_append_value(ketl_gc* p_gc, ketl_type* p_type, ketl_array* p_array, uint64_t value);

void ketl_gc_reg(ketl_gc* p_gc, void* p_object, ketl_type* p_type, uint64_t count, uint8_t flags);

uint64_t ketl_gc_get_allocation_count(ketl_gc* p_gc, void* p_object);

void ketl_gc_collect(ketl_gc* p_gc);

#endif // ketl_gc_memory_h
