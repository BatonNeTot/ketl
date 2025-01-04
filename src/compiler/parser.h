//🫖ketl
#ifndef ketl_compiler_parser_h
#define ketl_compiler_parser_h

#include "compiler/ir.h"

#include "ketl/memory.h"
#include "ketl/utils.h"


ketl_ir ketl_parser_parser(const char* pSource, uint32_t length, ketl_allocator* pAllocator);

#endif // ketl_compiler_parser_h