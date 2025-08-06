//🫖ketl
#include "ketl/ketl.hpp"

extern "C" {
}

#include <iostream>

int64_t factorial(int64_t val) {
    if (val <= 1) {
        return 1;
    }
    return val * factorial(val - 1);
}

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

    KETL::State ketl(&ketl_default_allocator);

    ketl.defineCFunction("factorial", &factorial);

    const char* pSource = 
        "i64 a = 5 + 8;"
        "i64 b = 34 - 6;"
        "a = b - a;"
        "return factorial((a - 1) * 4 / 10);"
    ;
    auto result = ketl.eval("", pSource);
    std::cout << "result = " << result << std::endl;
}
