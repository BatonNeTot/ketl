//🍲ketl
#ifndef compiler_lexer_h
#define compiler_lexer_h

#include <inttypes.h>
#include <stdbool.h>
#include <limits.h>

typedef struct KETLLexer_t* KETLLexer;
typedef const struct KETLLexer_t* KETLLexer_c;

typedef struct KETLToken_t* KETLToken;

#define KETL_LEXER_SOURCE_NULL_TERMINATED SIZE_MAX

KETLLexer ketlCreateLexer(const char* source, size_t length);

void ketlFreeLexer(KETLLexer lexer);

bool ketlHasNextToken(KETLLexer_c lexer);

KETLToken ketlGetNextToken(KETLLexer lexer);

void ketlFreeToken(KETLToken token);

#endif /*compiler_lexer_h*/
