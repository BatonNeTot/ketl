//🍲ketl
#ifndef compiler_lexer_h
#define compiler_lexer_h

#include "ketl/utils.h"

#include <stdbool.h>

KETL_FORWARD(KETLToken);
KETL_FORWARD(KETLObjectPool);

KETL_DEFINE(KETLLexer) {
	const char* source;
	const char* sourceIt;
	const char* sourceEnd;
	KETLObjectPool* tokenPool;
};

void ketlInitLexer(KETLLexer* lexer, const char* source, size_t length, KETLObjectPool* tokenPool);

bool ketlHasNextToken(const KETLLexer* lexer);

KETLToken* ketlGetNextToken(KETLLexer* lexer);

inline size_t ketl_lexer_current_position(KETLLexer* lexer) {
	return lexer->sourceIt - lexer->source;
}

#endif /*compiler_lexer_h*/
