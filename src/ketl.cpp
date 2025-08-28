//🫖ketl
#include "ketl/ketl.hpp"

#include <string>
#include <iostream>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
   
    KETL::State ketl(&ketl_default_allocator);

    std::string line;

    while (std::cin) {
        std::cout << ">> " << std::flush;
        std::getline(std::cin, line);
        auto result = ketl.eval("<eval>", { line });
        if (result) {
            std::cout << result << std::endl;
        }
    }

    return 0;
}
