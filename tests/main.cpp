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
        "var value : i64 = 5;\n"
        "if (value == 4) {\n"
        "   return 2;\n"
        "}\n"
        "return value;\n"
    ;

    auto result = ketl.eval(__FILE__ "$<eval>", p_source);
    if (result) {
        std::cout << "size = " << result.get_type().get_size() << std::endl;
        std::cout << "result = " << result << std::endl;
    }
}
