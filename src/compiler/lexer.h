//🍲ketl
#ifndef compiler_lexer_h
#define compiler_lexer_h

#include "ketl/common.h"

#include <stdbool.h>
#include <limits.h>

KETL_FORWARD(KETLLexer);
KETL_FORWARD(KETLToken);
KETL_FORWARD(KETLObjectPool);

#define KETL_LEXER_SOURCE_NULL_TERMINATED SIZE_MAX

struct KETLLexer {
	const char* source;
	const char* sourceIt;
	const char* sourceEnd;
	KETLObjectPool* tokenPool;
};

void ketlInitLexer(KETLLexer* lexer, const char* source, size_t length, KETLObjectPool* tokenPool);

bool ketlHasNextToken(const KETLLexer* lexer);

KETLToken* ketlGetNextToken(KETLLexer* lexer);

#endif /*compiler_lexer_h*/
