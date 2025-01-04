//🫖ketl
#include "ketl/ketl.hpp"

extern "C" {
    #include "compiler/parser.h"
    #include "compiler/bytecoder.h"
    #include "compiler/assembler.h"
    #include "executable_memory.h"
    #include "execution.h"
    #include "memory_impl.h"
}

#include <iostream>

#include <unordered_set>

class TestAllocator {
    public:

    inline bool verifyMemLeak() {
        return __allocated.empty();
    }
    inline bool verifyDoubleFree() {
        return __freedTwice.empty();
    }

    void reg(void* pointer) {
        __allocated.emplace(pointer);
    }

    void unreg(void* pointer) {
        auto it = __allocated.find(pointer);
        if (it == __allocated.end()) {
            __freedTwice.emplace(pointer);
        } else {
            __allocated.erase(it);
        }
    }

    private:

    std::unordered_set<void*> __freedTwice;
    std::unordered_set<void*> __allocated;
};

static void* test_alloc(size_t size, void* userInfo) {
	auto allocator = reinterpret_cast<TestAllocator*>(userInfo);
	auto ptr = malloc(size);
    allocator->reg(ptr);
    return ptr;
}

static void* test_realloc(void* ptr, size_t size, void* userInfo) {
	auto allocator = reinterpret_cast<TestAllocator*>(userInfo);
    allocator->unreg(ptr);
	ptr = realloc(ptr, size);
    allocator->reg(ptr);
    return ptr;
}

static void test_free(void* ptr, void* userInfo) {
	auto allocator = reinterpret_cast<TestAllocator*>(userInfo);
    allocator->unreg(ptr);
	free(ptr);
}

ketl_allocator test_allocator = {
	&test_alloc,
	&test_realloc,
	&test_free,
	NULL,
};

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

    auto testAllocator = new TestAllocator();
    test_allocator.userInfo = testAllocator;

    const char* source = "return 2 + 5 * 8;";
    auto ir = ketl_parser_parser(source, KETL_NULL_TERMINATED_LENGTH_32, &test_allocator);

    for (auto i = 0u; i < ir.nodesCount; ++i) {
        char buffer[256];
        auto length = ketl_ir_node_format(ir.pNodes[i], ir.pSymbols, buffer, sizeof(buffer) / sizeof(*buffer));
        printf("(%d) %.*s\n", i, length, buffer);
    }

    auto bytecode = ketl_bytecode_compile(ir, &test_allocator);
    ketl_free(&test_allocator, ir.pNodes);
    ketl_free(&test_allocator, ir.pSymbols);

    for (auto i = 0u; i < bytecode.instructionsCount; 
            i += ketl_bytecode_decode_instruction_length(bytecode.pInstructions[i])) {
        char buffer[256];
        auto length = ketl_bytecode_format(bytecode.pInstructions + i, bytecode.pLabels, buffer, sizeof(buffer) / sizeof(*buffer));
        printf("%d: %.*s\n", i, length, buffer);
    }

    uint32_t opcodesSize = 0u;
    auto pOpcodes = ketl_assembler_compile(bytecode, &opcodesSize, &test_allocator);
    ketl_free(&test_allocator, bytecode.pInstructions);

    {
        char buffer[1024];
        auto length = ketl_assembler_format(pOpcodes, opcodesSize, buffer, sizeof(buffer) / sizeof(*buffer));
        printf("%.*s\n", length, buffer);
    }

    ketl_executable_memory ex_memory;
    ketl_executable_memory_init(&ex_memory, &test_allocator);

    auto executableOpcodes = ketl_executable_memory_allocate(&ex_memory, pOpcodes, opcodesSize);
    ketl_free(&test_allocator, pOpcodes);
    int result = ketl_execute(&executableOpcodes);
    printf("result = %d\n", result);

    ketl_executable_memory_deinit(&ex_memory);

    std::cout << (testAllocator->verifyMemLeak() ? "Yeah!" : "Ooh") << std::endl;
    std::cout << (testAllocator->verifyDoubleFree() ? "Yeah!" : "Ooh") << std::endl;
}
