//🍲ketl
#ifndef compiler_syntax_parser_h
#define compiler_syntax_parser_h

#include "ketl/utils.h"

KETL_FORWARD(KETLObjectPool);
KETL_FORWARD(KETLStackIterator);
KETL_FORWARD(KETLSyntaxNode);

void ketl_count_lines(const char* source, uint64_t length, uint32_t* pLine, uint32_t* pColumn);

KETLSyntaxNode* ketlParseSyntax(KETLObjectPool* syntaxNodePool, KETLStackIterator* bnfStackIterator, const char* source);

#endif /*compiler_syntax_parser_h*/
