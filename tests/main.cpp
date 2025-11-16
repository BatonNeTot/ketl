//🫖ketl
#include "ketl/ketl.hpp"

extern "C" {
}

#include <fstream>
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

    std::string testfile_name = "tests/test.ktl";
    auto testfile_size = std::filesystem::file_size(testfile_name);
    std::string source(testfile_size, '\0');
    std::ifstream testfile_in(testfile_name);
    testfile_in.read(source.data(), testfile_size);
    testfile_in.close();

    auto result = ketl.eval(testfile_name, source);
    if (result) {
        std::cout << "size = " << result.get_type().get_size() << std::endl;
        std::cout << "result = " << result << std::endl;
    }
}
