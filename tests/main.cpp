//🫖ketl
#include "ketl/ketl.hpp"

extern "C" {
#include "containers/tree_map.h"

#include "gc_memory.h"
}

#include <iostream>

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

    const char* source = "return 100 + 7 * 7;";
    
    auto state = ketl_state_create(&ketl_default_allocator);
    auto result = ketl_state_eval_int64(state, source, KETL_NULL_TERMINATED_LENGTH_32);
    std::cout << "result = " << result << std::endl;
    ketl_state_destroy(state);
}
