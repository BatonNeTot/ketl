//🫖ketl
#ifndef ketl_compiler_lexer_h
#define ketl_compiler_lexer_h

#include "compiler/token.h"

#include "ketl/memory.h"
#include "ketl/utils.h"


#define KETL_LEXER_INITIAL_TOKEN_CAPACITY 4

ketl_token* ketl_lexer_build_tokens(const char* pSource, uint32_t length, uint32_t* pCount, const ketl_allocator* pAllocator);

#endif // ketl_compiler_lexer_h
