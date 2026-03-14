//🫖ketl
#include "ketl/ketl.hpp"

#include <string>
#include <iostream>

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
        puts("Can't open file 'data'\n");
        exit(1);
    }

    // stdout now refers to file p_target_filename
    if (-1 == _dup2(_fileno(p_redirect), 1)) {
        perror("Can't _dup2 stdout");
        exit(1);
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

    redirect_init();
    for (int i = 1; i < argc; ++i) {
        char a_buffer[256] = {'\0'};

        size_t length = strlen(argv[i]);
        memcpy(a_buffer, argv[i], length);
        memcpy(a_buffer + length, ".s", 2);

        redirect_stdout(a_buffer);
        KETL::State ketl(&ketl_default_allocator);

        ketl.module_print_asm(std::string_view{argv[i], length});
    }
    redirect_restore();

    return 0;
}
