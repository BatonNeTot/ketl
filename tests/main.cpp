//🫖ketl
#include "ketl/ketl.hpp"

extern "C" {

typedef struct array_t {
    const void* data;
    uint64_t capacity;
    uint64_t size;
    bool is_slice;
} array_t;

array_t* eval(array_t* p_filename, array_t* p_source);

#include "compiler/lexer.h"
}

#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

    //*
    const char* p_filename = "test_case.ktl";
    uint64_t filename_length = strlen(p_filename);
    array_t str_filename = {
        .data = reinterpret_cast<const void*>(p_filename),
        .capacity = filename_length,
        .size = filename_length,
        .is_slice = false,
    };

    std::ifstream file(p_filename, std::ios::binary);
    std::string source;

    file.seekg(0, std::ios::end);   
    source.reserve(file.tellg());
    file.seekg(0, std::ios::beg);

    source.assign((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());

    const char* p_source = source.c_str();
    uint64_t source_length = strlen(p_source);
    array_t str_source = {
        .data = reinterpret_cast<const void*>(p_source),
        .capacity = source_length,
        .size = source_length,
        .is_slice = false,
    };

    (void)str_filename;
    (void)str_source;
    array_t* result = eval(&str_filename, &str_source);
    (void)result;

    ketl_lexer_t test_lexer;
    ketl_lexer_init(&test_lexer, &ketl_default_allocator);
    ketl_lexer_build_tokens(&test_lexer, KETL_ATOMIC_STRING_EMPTY, p_source, source_length);

    return 0;
    //*/

    /*
    KETL::State ketl(&ketl_default_allocator);

    std::string testfile_name = "test_case";

    ketl.load_module(testfile_name);
    //*/

    /*
    KETL::State ketl(&ketl_default_allocator);

    //ketl.define_cfunction("test", static_cast<int64_t(*)(int64_t)>(&test));
    //ketl.define_cfunction("test", static_cast<int64_t(*)(int64_t, int64_t)>(&test));

    std::string testfile_name = "test.ktl";
    auto testfile_size = std::filesystem::file_size(testfile_name);
    std::string source(testfile_size, '\0');
    std::ifstream testfile_in(testfile_name, std::ios::binary);
    testfile_in.read(source.data(), testfile_size);
    testfile_in.close();
    
    auto result = ketl.eval(source);
    if (result) {
        std::cout << "size = " << result.get_type().get_size() << std::endl;
        std::cout << "result = " << result << std::endl;
    }
    //*/

    /*
    std::string source1 = "fn get_funky() -> i64 { return 42; }";
    std::string source2 = "return get_funky(\n\n);";

    ketl.eval(source1);
    auto result = ketl.eval(source2);
    if (result) {
        std::cout << "size = " << result.get_type().get_size() << std::endl;
        std::cout << "result = " << result << std::endl;
    }
    //*/
}
