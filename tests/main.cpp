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

    const char* p_source = ""
        "var a : i64 = 5;\n"
        "if (a == 5) {\n"
        "   return 1;\n"
        "   return 42;\n"
        "} else {\n"
        "   return 2;\n" 
        "}"
    ;

    auto result = ketl.eval(__FILE__ "$<eval>", p_source);
    if (result) {
        std::cout << "result = " << result << std::endl;
    }
}
