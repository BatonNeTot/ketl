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

    //ketl.defineCFunction("test", static_cast<int64_t(*)(int64_t)>(&test));
    ketl.defineCFunction("test", static_cast<int64_t(*)(int64_t, int64_t)>(&test));

    const char* pSource = ""
        "var a : i64 = 5;"
        "if (a == 5) {"
        //"   return 1;"
        "} else {"
        //"   return 2;" 
        "}"
    ;

    auto result = ketl.eval("", pSource);
    if (result) {
        std::cout << "result = " << result << std::endl;
    }
}
