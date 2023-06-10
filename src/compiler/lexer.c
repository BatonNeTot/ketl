//🍲ketl
#include "lexer.h"

#include "token.h"

#include "assert.h"
#include "common.h"

#include <stdlib.h>

struct KETLLexer_t {
	const char* source;
	const char* sourceIt;
	const char* sourceEnd;
};

#define KETL_CHECK_LEXER_MESSAGE "Lexer is null"

#define KETL_CHECK_LEXER if (KETL_CHECK_VOEM(lexer, KETL_CHECK_LEXER_MESSAGE)) {\
	return;\
}

#define KETL_CHECK_LEXER_VALUE(x) if (KETL_CHECK_VOEM(lexer, KETL_CHECK_LEXER_MESSAGE)) {\
	return (x);\
}


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
	return value = '\n';
}


static inline void iterate(KETLLexer lexer) {
	++lexer->sourceIt;
}

static inline char getChar(KETLLexer lexer) {
	return lexer->sourceIt < lexer->sourceEnd ? *(lexer->sourceIt) : 0;
}

static inline char getNextChar(KETLLexer_c lexer) {
	const char* pNext = lexer->sourceIt + 1;
	return pNext < lexer->sourceEnd ? *(pNext) : 0;
}

static inline char getCharAndIterate(KETLLexer lexer) {
	if (lexer->sourceIt >= lexer->sourceEnd) {
		lexer->sourceIt = lexer->sourceEnd;
		return '\0';
	}

	return *(lexer->sourceIt++);
}


static inline void skipSingleLineComment(KETLLexer lexer) {
	iterate(lexer);
	iterate(lexer);

	KETL_FOREVER {
		char current = getChar(lexer);

		if (current == '\0') {
			return;
		}

		if (isNextLineSymbol(current)) {
			iterate(lexer);
			return;
		}
	}
}

static inline void skipMultiLineComment(KETLLexer lexer) {
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

static inline void skipSpaceAndComments(KETLLexer lexer) {
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


static KETLToken createToken(KETLLexer_c lexer, const char* startIt, KETLTokenType type) {
	KETLToken token = malloc(sizeof(struct KETLToken_t));
	if (KETL_CHECK_VOEM(token, "Can't allocate space for token")) {
		return NULL;
	}

	ptrdiff_t length = lexer->sourceIt - startIt;

	if (KETL_CHECK_VOEM(length <= UINT16_MAX, "Token length is bigger then "KETL_STR_VALUE(UINT16_MAX))) {
		return NULL;
	}

	token->value = startIt;
	token->length = (uint16_t)length;
	token->type = type;
	token->positionInSource = startIt - lexer->source;

	return token;
}

static inline KETLToken createTokenAndSkip(KETLLexer lexer, const char* startIt, KETLTokenType type) {
	KETLToken token = createToken(lexer, startIt, type);
	if (!token) {
		return NULL;
	}

	skipSpaceAndComments(lexer);

	return token;
}

void ketlFreeToken(KETLToken token) {
	free(token);
}

static inline bool hasNextChar(KETLLexer_c lexer) {
	return lexer->sourceIt < lexer->sourceEnd && *lexer->sourceIt != '\0';
}

KETLLexer ketlCreateLexer(const char* source, size_t length) {
	KETLLexer lexer = malloc(sizeof(struct KETLLexer_t));
	if (KETL_CHECK_VOEM(lexer, "Can't allocate space for lexer")) {
		return NULL;
	}

	lexer->source = source;
	lexer->sourceIt = source;
	lexer->sourceEnd = length == KETL_LEXER_SOURCE_NULL_TERMINATED ? (void*)(-1) : source + length;

	skipSpaceAndComments(lexer);

	return lexer;
}

void ketlFreeLexer(KETLLexer lexer) {
	KETL_CHECK_LEXER;

	free(lexer);
}


bool ketlHasNextToken(KETLLexer_c lexer) {
	KETL_CHECK_LEXER_VALUE(false);

	return hasNextChar(lexer);
}

static inline KETLToken checkLeftoverSpecial(KETLLexer_c lexer, const char* startIt) {
	if (startIt < lexer->sourceIt) {
		return createToken(lexer, startIt, KETL_TOKEN_TYPE_SPECIAL);
	}
	return NULL;
}

KETLToken ketlGetNextToken(KETLLexer lexer) {
	KETL_CHECK_LEXER_VALUE(NULL);

	const char* startIt = lexer->sourceIt;
	char current = getChar(lexer);

	KETL_FOREVER {
		if (current == '\0') {
			break;
		}

		if (isSpace(current)) {
			KETLToken token = createToken(lexer, startIt, KETL_TOKEN_TYPE_SPECIAL);
			iterate(lexer);
			skipSpaceAndComments(lexer);
			return token;
		}

		if (isFirstSymbolComment(current)) {
			char next = getNextChar(lexer);
			if (isSecondSymbolSingleLineComment(next)) {
				KETLToken token = createToken(lexer, startIt, KETL_TOKEN_TYPE_SPECIAL);

				skipSingleLineComment(lexer);
				skipSpaceAndComments(lexer);
				return token;
			}
			if (isSecondSymbolMultiLineComment(next)) {
				KETLToken token = createToken(lexer, startIt, KETL_TOKEN_TYPE_SPECIAL);

				skipMultiLineComment(lexer);
				skipSpaceAndComments(lexer);
				return token;
			}
			iterate(lexer);
			current = getChar(lexer);
			continue;
		}

		if (isNumberDot(current) && isNumber(getNextChar(lexer))) {
			KETLToken token = checkLeftoverSpecial(lexer, startIt);
			if (token) { return token; }
			iterate(lexer);
			do iterate(lexer); while (isNumber(getChar(lexer)));
			return createTokenAndSkip(lexer, startIt, KETL_TOKEN_TYPE_NUMBER);
		}

		if (isNumber(current)) {
			KETLToken token = checkLeftoverSpecial(lexer, startIt);
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
			KETLToken token = checkLeftoverSpecial(lexer, startIt);
			if (token) { return token; }
			iterate(lexer);
			startIt = lexer->sourceIt;
			KETL_FOREVER {
				char current = getChar(lexer);
				if (KETL_CHECK_VOEM(current != '\0', "Unexpected EOF")) {
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
			KETLToken token = checkLeftoverSpecial(lexer, startIt);
			if (token) { return token; }
			do iterate(lexer); while (isIdSymbol(getChar(lexer)));
			return createTokenAndSkip(lexer, startIt, KETL_TOKEN_TYPE_ID);
		}

		iterate(lexer);
		current = getChar(lexer);
	}

	return createTokenAndSkip(lexer, startIt, KETL_TOKEN_TYPE_SPECIAL);
}

#undef KETL_CHECK_LEXER
#undef KETL_CHECK_LEXER_VALUE
#undef KETL_CHECK_LEXER_MESSAGE