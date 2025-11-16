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

    //ketl.define_cfunction("test", static_cast<int64_t(*)(int64_t)>(&test));
    //ketl.define_cfunction("test", static_cast<int64_t(*)(int64_t, int64_t)>(&test));

    ketl.define_class("TEST", 
        ketl.create_field<int64_t>("value1"), 
        ketl.create_field<int64_t>("value2"));

    const char* p_source = ""
        "var test = TEST();\n"
        "test.value1 = 4;"
        "test.value2 = 2;"
        "return;\n"
    ;
    //const char* p_source = ""
    //    "return 5 * 6 + 3 * 4;"
    //;

    auto result = ketl.eval(__FILE__ "$<eval>", p_source);
    if (result) {
        std::cout << "size = " << result.get_type().get_size() << std::endl;
        std::cout << "result = " << result << std::endl;
    }
}
