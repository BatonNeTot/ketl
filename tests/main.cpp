//🫖ketl
#include "ketl/ketl.hpp"

extern "C" {
}

#include <iostream>

int64_t test(int64_t val) {
    return val + 42000;
}

int64_t test(int64_t a, int64_t b) {
    return a * 1000 + b;
}

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

    KETL::State ketl(&ketl_default_allocator);

    ketl.defineCFunction("test", static_cast<int64_t(*)(int64_t)>(&test));
    //ketl.defineCFunction("test", static_cast<int64_t(*)(int64_t, int64_t)>(&test));

    const char* pSource = 
        "i64 a = 5 + 8;"
        "i64 b = 34 - 6;"
        "a = (b - a) % 20;"
        "i64 result = 0;"
        "if (((a - 1) * 4 / 10) == 5) {"
        "   result = 42;"
        "} else {"
        "   result = 31;"
        "}"
        "return result;"
    ;
    auto result = ketl.eval("", pSource);
    std::cout << "result = " << result << std::endl;
}
