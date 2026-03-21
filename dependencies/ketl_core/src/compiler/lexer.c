//🍲ketl
#include "lexer.h"

#include "token.h"

#include "ketl/assert.h"
#include "ketl/utils.h"
#include "ketl/object_pool.h"

#include <inttypes.h>
#include <stdlib.h>

#define KETL_CHECK_LEXER_MESSAGE "Lexer is null"

#define KETL_CHECK_LEXER if (KETL_CHECK_VOEM(lexer, KETL_CHECK_LEXER_MESSAGE, 0)) {\
	return;\
}

#define KETL_CHECK_LEXER_VALUE(x) if (KETL_CHECK_VOEM(lexer, KETL_CHECK_LEXER_MESSAGE, 0)) {\
	return (x);\
}


size_t ketl_lexer_current_position(KETLLexer* lexer);

static inline bool isSpace(char value) {
	return value == ' '
		|| (value >= '\t' && value <= '\r');
}

static inline bool isStringQuote(char value) {
	return value == '"';
}

static inline bool isNumberDot(char value) {
	return value == '.';
}

static inline bool isNumber(char value) {
	return value >= '0' && value <= '9';
}

static inline bool isStartingIdSymbol(char value) {
	return value == '_'
		|| (value >= 'a' && value <= 'z')
		|| (value >= 'A' && value <= 'Z');
}
static inline bool isIdSymbol(char value) {
	return isStartingIdSymbol(value)
		|| isNumber(value);
}

static inline bool isFirstSymbolComment(char value) {
	return value == '/';
}

static inline bool isSecondSymbolSingleLineComment(char value) {
	return value == '/';
}
static inline bool isSecondSymbolMultiLineComment(char value) {
	return value == '*';
}

static inline bool isFirstSymbolMultiLineCommentEnd(char value) {
	return value == '*';
}
static inline bool isSecondSymbolMultiLineCommentEnd(char value) {
	return value == '/';
}

static inline bool isNextLineSymbol(char value) {
	return value == '\n';
}


static inline void iterate(KETLLexer* lexer) {
	++lexer->sourceIt;
}

static inline bool hasNextCharAfter(const KETLLexer* lexer, const char* sourceIt) {
	const char* sourceEnd = lexer->sourceEnd;
	return sourceEnd == NULL ? *sourceIt != '\0' : sourceIt < sourceEnd;
}

static inline bool hasNextChar(const KETLLexer* lexer) {
	return hasNextCharAfter(lexer, lexer->sourceIt);
}

static inline char getChar(const KETLLexer* lexer) {
	return hasNextChar(lexer) ? *(lexer->sourceIt) : '\0';
}

static inline char getNextChar(const KETLLexer* lexer) {
	const char* pNext = lexer->sourceIt + 1;
	return hasNextCharAfter(lexer, pNext) ? *(pNext) : '\0';
}

static inline char getCharAndIterate(KETLLexer* lexer) {
	if (!hasNextChar(lexer)) {
		return '\0';
	}

	return *(lexer->sourceIt++);
}


static inline void skipSingleLineComment(KETLLexer* lexer) {
	iterate(lexer);
	iterate(lexer);

	KETL_FOREVER {
		char current = getChar(lexer);

		if (current == '\0') {
			return;
		}

		iterate(lexer);

		if (isNextLineSymbol(current)) {
			return;
		}
	}
}

static inline void skipMultiLineComment(KETLLexer* lexer) {
	iterate(lexer);
	iterate(lexer);

	char current = getChar(lexer);
	KETL_FOREVER{
		if (current == '\0') {
			return;
		}

		if (isFirstSymbolMultiLineCommentEnd(current)) {
			iterate(lexer);
			current = getChar(lexer);
			if (isSecondSymbolMultiLineCommentEnd(current)) {
				return;
			}
		}
		else {
			iterate(lexer);
			current = getChar(lexer);
		}
	}
}

static inline void skipSpaceAndComments(KETLLexer* lexer) {
	KETL_FOREVER{
		char current = getChar(lexer);
		if (current == '\0') {
			return;
		}

		if (isFirstSymbolComment(current)) {
			if (isSecondSymbolSingleLineComment(getNextChar(lexer))) {
				skipSingleLineComment(lexer);
				continue;
			}
			if (isSecondSymbolMultiLineComment(getNextChar(lexer))) {
				skipMultiLineComment(lexer);
				continue;
			}
			return;
		}

		if (!isSpace(current)) {
			return;
		}

		iterate(lexer);
	}
}


static KETLToken* createToken(const KETLLexer* lexer, const char* startIt, KETLTokenType type) {
	KETLToken* token = ketlGetFreeObjectFromPool(lexer->tokenPool);
	if (KETL_CHECK_VOEM(token, "Can't allocate space for token", 0)) {
		return NULL;
	}

	ptrdiff_t length = lexer->sourceIt - startIt;

	if (KETL_CHECK_VOEM(length <= UINT16_MAX, "Token length is bigger then "KETL_STR_VALUE(UINT16_MAX), 0)) {
		return NULL;
	}

	token->value = startIt;
	token->length = (uint16_t)length;
	token->type = type;
	token->positionInSource = startIt - lexer->source;

	return token;
}

static inline KETLToken* createTokenAndSkip(KETLLexer* lexer, const char* startIt, KETLTokenType type) {
	KETLToken* token = createToken(lexer, startIt, type);
	if (!token) {
		return NULL;
	}

	skipSpaceAndComments(lexer);

	return token;
}

void ketlInitLexer(KETLLexer* lexer, const char* source, size_t length, KETLObjectPool* tokenPool) {
	lexer->source = source;
	lexer->sourceIt = source;
	lexer->sourceEnd = length >= KETL_NULL_TERMINATED_LENGTH ? NULL : source + length;
	lexer->tokenPool = tokenPool;

	skipSpaceAndComments(lexer);
}


bool ketlHasNextToken(const KETLLexer* lexer) {
	KETL_CHECK_LEXER_VALUE(false);

	return hasNextChar(lexer);
}

static inline KETLToken* checkLeftoverSpecial(const KETLLexer* lexer, const char* startIt) {
	if (startIt < lexer->sourceIt) {
		return createToken(lexer, startIt, KETL_TOKEN_TYPE_SPECIAL);
	}
	return NULL;
}

KETLToken* ketlGetNextToken(KETLLexer* lexer) {
	KETL_CHECK_LEXER_VALUE(NULL);

	const char* startIt = lexer->sourceIt;
	char current = getChar(lexer);

	KETL_FOREVER {
		if (current == '\0') {
			break;
		}

		if (isSpace(current)) {
			KETLToken* token = createToken(lexer, startIt, KETL_TOKEN_TYPE_SPECIAL);
			iterate(lexer);
			skipSpaceAndComments(lexer);
			return token;
		}

		if (isFirstSymbolComment(current)) {
			char next = getNextChar(lexer);
			if (isSecondSymbolSingleLineComment(next)) {
				KETLToken* token = createToken(lexer, startIt, KETL_TOKEN_TYPE_SPECIAL);

				skipSingleLineComment(lexer);
				skipSpaceAndComments(lexer);
				return token;
			}
			if (isSecondSymbolMultiLineComment(next)) {
				KETLToken* token = createToken(lexer, startIt, KETL_TOKEN_TYPE_SPECIAL);

				skipMultiLineComment(lexer);
				skipSpaceAndComments(lexer);
				return token;
			}
			iterate(lexer);
			current = getChar(lexer);
			continue;
		}

		if (isNumberDot(current) && isNumber(getNextChar(lexer))) {
			KETLToken* token = checkLeftoverSpecial(lexer, startIt);
			if (token) { return token; }
			iterate(lexer);
			do iterate(lexer); while (isNumber(getChar(lexer)));
			return createTokenAndSkip(lexer, startIt, KETL_TOKEN_TYPE_NUMBER);
		}

		if (isNumber(current)) {
			KETLToken* token = checkLeftoverSpecial(lexer, startIt);
			if (token) { return token; }
			iterate(lexer);
			KETL_FOREVER {
				char current = getChar(lexer);
				if (isNumber(current)) {
					iterate(lexer);
					continue;
				}
				if (isNumberDot(current)) {
					iterate(lexer);
				}
				break;
			};
			while (isNumber(getChar(lexer))) { iterate(lexer); }
			return createTokenAndSkip(lexer, startIt, KETL_TOKEN_TYPE_NUMBER);
		}

		if (isStringQuote(current)) {
			KETLToken* token = checkLeftoverSpecial(lexer, startIt);
			if (token) { return token; }
			iterate(lexer);
			startIt = lexer->sourceIt;
			KETL_FOREVER {
				char current = getChar(lexer);
				if (KETL_CHECK_VOEM(current != '\0', "Unexpected EOF", 0)) {
					return NULL;
				}
				if (isStringQuote(current)) {
					break;
				}
				iterate(lexer);
			}
			token = createToken(lexer, startIt, KETL_TOKEN_TYPE_STRING);
			iterate(lexer);
			skipSpaceAndComments(lexer);
			return token;
		}

		if (isStartingIdSymbol(current)) {
			KETLToken* token = checkLeftoverSpecial(lexer, startIt);
			if (token) { return token; }
			do iterate(lexer); while (isIdSymbol(getChar(lexer)));
			return createTokenAndSkip(lexer, startIt, KETL_TOKEN_TYPE_ID);
		}

		iterate(lexer);
		current = getChar(lexer);
	}

	return createTokenAndSkip(lexer, startIt, KETL_TOKEN_TYPE_SPECIAL);
}