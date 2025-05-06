//🫖ketl
#include "ketl/ketl.hpp"

extern "C" {
}

#include <iostream>

int64_t inc(int64_t val) {
    return ++val;
}

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

    KETL::State ketl(&ketl_default_allocator);

    ketl.defineFunction("inc", &inc);

    const char* pSource = "return inc(2);";
    auto result = ketl.eval(pSource);
    std::cout << "result = " << result << std::endl;
}
