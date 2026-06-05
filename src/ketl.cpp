//🫖ketl
#include "ketl/ketl.hpp"

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

#include <io.h>

static int stdout_backup = 0;
static FILE* p_redirect = NULL;

void redirect_init() {
    stdout_backup = _dup(_fileno(stdout));
}

void redirect_restore() {
    fflush(stdout);
    if (p_redirect != NULL) {
        fclose(p_redirect);
    }
    _dup2(stdout_backup, _fileno(stdout));
}

void redirect_stdout(const char* p_target_filename) {
    redirect_restore();
    
    if (fopen_s(&p_redirect, p_target_filename, "w") != 0 ) {
        printf("Can't open file '%s'\n", p_target_filename);
        exit(1);
    }

    // stdout now refers to file p_target_filename
    if (-1 == _dup2(_fileno(p_redirect), 1)) {
        perror("Can't _dup2 stdout");
        exit(1);
    }
}

void compile_asm_file(const char* p_filepath) {
    redirect_init();
    class _defer{ public: ~_defer() {
        redirect_restore();
    }} redirect_deffer;

    char a_buffer[256];

    size_t length = strlen(p_filepath);
    size_t after_last_dot_index = length;
    while (after_last_dot_index != 0 && p_filepath[after_last_dot_index - 1] != '.') {
        --after_last_dot_index;
    }
    
    if (strcmp(p_filepath + after_last_dot_index, "ktl") != 0 || after_last_dot_index == 0) {
        return;
    }

    snprintf(a_buffer, ANN_ARRAY_SIZE(a_buffer), "%.*ss", (int)after_last_dot_index, p_filepath);

    redirect_stdout(a_buffer);
    KETL::State ketl(&ketl_default_allocator);

    ketl.print_compile2asm(std::string_view{p_filepath, length});
}

void compile_asm(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return;
    }

    if (std::filesystem::is_directory(path)) {
        for (auto it = std::filesystem::directory_iterator(path); it != std::filesystem::directory_iterator(); ++it) {
            compile_asm(*it);
        }
        return;
    }

    if (std::filesystem::is_regular_file(path)) {
        compile_asm_file(path.string().c_str());
    }
}

void compile_asm_list(int amount, char **pp_paths) {
    for (int i = 0; i < amount; ++i) {
        const char* p_path = pp_paths[i]; 

        compile_asm(p_path);
    }
}

void repl() {
    KETL::State ketl(&ketl_default_allocator);

    std::string line;

    while (std::cin) {
        std::cout << ">> " << std::flush;
        std::getline(std::cin, line);
        auto result = ketl.eval( { line });
        if (result) {
            std::cout << result << std::endl;
        }
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (argc <= 1) {
        repl();
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "-S") == 0) {
        compile_asm_list(argc - 2, argv + 2);

        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        char a_buffer[256] = {'\0'};

        size_t length = strlen(argv[i]);
        memcpy(a_buffer, argv[i], length);

        KETL::State ketl(&ketl_default_allocator);

        ketl.load_module(std::string_view{argv[i], length});
    }

    return 0;
}
