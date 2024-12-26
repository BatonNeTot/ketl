//🫖ketl
#include "lexer.h"
#include <stdlib.h>
#include <string.h>

static inline bool ketl_lexer_is_space(char symbol) {
    return symbol == ' ' || (symbol >= '\t' && symbol <= '\r');
}

static inline bool ketl_lexer_is_numeric(char symbol) {
    return symbol >= '0' && symbol <= '9';
}

static inline bool ketl_lexer_is_alpha(char symbol) {
    return (symbol >= 'a' && symbol <= 'z') || (symbol >= 'A' && symbol <= 'Z');
}

KETL_DEFINE(ketl_lexer_context) {
    const char* pSource;
    uint32_t length;
    uint32_t offset;
    uint32_t count;
    uint32_t capacity;
    ketl_token* pTokens;
    uint32_t lastTokenEnd;
};

static void ketl_lexer_add_token(ketl_lexer_context* pContext, ketl_token_type type, uint32_t length) {
    uint32_t count = pContext->count;
    if (count < pContext->capacity) {
        uint32_t newCapacity = pContext->capacity = (uint32_t)(pContext->capacity * 1.5f);
        pContext->pTokens = realloc(pContext->pTokens, sizeof(ketl_token) * newCapacity);
    }
    uint32_t offset = pContext->offset;
    uint32_t prevOffset = offset - pContext->lastTokenEnd;
    pContext->pTokens[count] = (ketl_token){.type = type, .length = length, .prevOffset = prevOffset};
    ++pContext->count;
    pContext->lastTokenEnd = offset + length;
}

static char ketl_lexer_get_symbol(ketl_lexer_context* pContext) {
    char symbol = '\0';
    uint32_t offset = pContext->offset;
    if (offset < pContext->length) {
        symbol = pContext->pSource[offset];
    }
    return symbol;
}

static bool ketl_lexer_parse_next_line(ketl_lexer_context* pContext, char nextSymbol) {
    if (nextSymbol == '\r') {
        uint32_t offset = pContext->offset;
        pContext->offset += offset + 1 < pContext->length && pContext->pSource[offset + 1] == '\n' ? 2 : 1;
        return true;
    }

    if (nextSymbol == '\n') {
        pContext->offset += 1;
        return true;
    }

    return false;
}

static bool ketl_lexer_parse_comments(ketl_lexer_context* pContext, char nextSymbol) {
    uint32_t offset = pContext->offset;
    if (nextSymbol != '/' || offset + 1 >= pContext->length) {
        return false;
    }

    nextSymbol = pContext->pSource[offset + 1];

    // single line comment
    if (nextSymbol == '/') {
        pContext->offset += 2;
        KETL_FOREVER {
            nextSymbol = ketl_lexer_get_symbol(pContext);
            if (nextSymbol == '\0' || ketl_lexer_parse_next_line(pContext, nextSymbol)) {
                break;
            }
            pContext->offset += 1;
        }
        return true;
    }

    // multiline comment
    if (nextSymbol == '*') {
        pContext->offset += 2;
        KETL_FOREVER {
            nextSymbol = ketl_lexer_get_symbol(pContext);
            if (nextSymbol == '\0') {
                // TODO ERROR
                break;
            }

            if (nextSymbol == '*' && pContext->offset + 1 < pContext->length && pContext->pSource[pContext->offset + 1] == '/') {
                pContext->offset += 2;
                break;
            }

            pContext->offset += 1;
        }

        return true;
    }

    return false;
}

static bool ketl_lexer_parse_literal_char(ketl_lexer_context* pContext, char nextSymbol) {
    if (nextSymbol != '\'') {
        return false;
    }

    uint32_t charLength = 1;
    uint32_t charOffset = pContext->offset += 1;

    if (ketl_lexer_get_symbol(pContext) == '\\') {
        ++charLength;
    }

    uint32_t endMarkOffset = charOffset + charLength;

    // error correction
    if (endMarkOffset >= pContext->length || pContext->pSource[endMarkOffset] != '\'') {
        if (endMarkOffset >= pContext->length) {
            charLength = pContext->length - charOffset;
        }
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LITERAL_CHAR, charLength);
        pContext->offset += charLength;
        return true;
    }

    ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LITERAL_CHAR, charLength);
    pContext->offset += charLength + 1;
    return true;
}

static bool ketl_lexer_parse_literal_string(ketl_lexer_context* pContext, char nextSymbol) {
    if (nextSymbol != '"') {
        return false;
    }

    char literalStartOffset = pContext->offset += 1;
    KETL_FOREVER {
        nextSymbol = ketl_lexer_get_symbol(pContext);
        if (nextSymbol == '\0') {
            // TODO ERROR
            break;
        }

        
        if (nextSymbol == '*') {
            pContext->offset += 1;
            break;
        }

        pContext->offset += 1;
    }

    uint32_t literalLength = pContext->offset - literalStartOffset;
    pContext->offset = literalStartOffset;
    ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LITERAL_STRING, literalLength);
    pContext->offset += literalLength + 1;
    return true;
}

static bool ketl_lexer_parse_literal_integer(ketl_lexer_context* pContext, char nextSymbol) {
    if (!ketl_lexer_is_numeric(nextSymbol)) {
        return false;
    }

    char literalStartOffset = pContext->offset;
    pContext->offset = literalStartOffset + 1;


    KETL_FOREVER {
        nextSymbol = ketl_lexer_get_symbol(pContext);
        if (!ketl_lexer_is_numeric(nextSymbol)) {
            break;
        }
        
        pContext->offset += 1;
    }

    uint32_t literalLength = pContext->offset - literalStartOffset;
    pContext->offset = literalStartOffset;
    ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LITERAL_INTEGER, literalLength);
    pContext->offset += literalLength;
    return true;
}

static bool ketl_lexer_parse_id(ketl_lexer_context* pContext, char nextSymbol) {
    if (nextSymbol != '_' && !ketl_lexer_is_alpha(nextSymbol)) {
        return false;
    }

    char idStartOffset = pContext->offset;
    pContext->offset = idStartOffset + 1;

    KETL_FOREVER {
        nextSymbol = ketl_lexer_get_symbol(pContext);
        if (nextSymbol != '_' && !ketl_lexer_is_alpha(nextSymbol) && !ketl_lexer_is_numeric(nextSymbol)) {
            break;
        }
        
        pContext->offset += 1;
    }

    uint32_t idLength = pContext->offset - idStartOffset;
    pContext->offset = idStartOffset;
    ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ID, idLength);
    pContext->offset += idLength;
    return true;
}

static bool ketl_lexer_parse_operator(ketl_lexer_context* pContext, char nextSymbol) {
    switch (nextSymbol) {  
    case '(': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, 1);
        pContext->offset += 1;
        return true;
    } 
	case ')': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, 1);
        pContext->offset += 1;
        return true;
    } 
	case '{': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_CURLY_LEFT, 1);
        pContext->offset += 1;
        return true;
    } 
	case '}': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_CURLY_RIGHT, 1);
        pContext->offset += 1;
        return true;
    } 
	case '[': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_SQUARE_LEFT, 1);
        pContext->offset += 1;
        return true;
    } 
	case ']': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_SQUARE_RIGHT, 1);
        pContext->offset += 1;
        return true;
    } 
	case '.': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_DOT, 1);
        pContext->offset += 1;
        return true;
    } 
	case ',': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_COMMA, 1);
        pContext->offset += 1;
        return true;
    } 
	case '?': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_TERNARY_FIRST, 1);
        pContext->offset += 1;
        return true;
    } 
	case ':': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_TERNARY_SECOND, 1);
        pContext->offset += 1;
        return true;
    } 
	case ';': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, 1);
        pContext->offset += 1;
        return true;
    } 
	case '~': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_NOT, 1);
        pContext->offset += 1;
        return true;
    } 
	case '=': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;

        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_EQUAL, 2);
            pContext->offset += 2;
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN, 1);
            pContext->offset += 1;
        }
        return true;
    } 
	case '^': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_BITWISE_XOR, 2);
            pContext->offset += 2;
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_XOR, 1);
            pContext->offset += 1;
        }
        return true;
    } 
	case '*': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_MULTIPLY, 2);
            pContext->offset += 2;
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_MULTIPLY, 1);
            pContext->offset += 1;
        }
        return true;
    } 
	case '/': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_DIVIDE, 2);
            pContext->offset += 2;
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_DIVIDE, 1);
            pContext->offset += 1;
        }
        return true;
    } 
	case '%': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_REMAINDER, 2);
            pContext->offset += 2;
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_REMAINDER, 1);
            pContext->offset += 1;
        }
        return true;
    } 
	case '!': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_NOT_EQUAL, 2);
            pContext->offset += 2;
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LOGICAL_NOT, 1);
            pContext->offset += 1;
        }
        return true;
    } 
	case '+': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_PLUS, 2);
            pContext->offset += 2;
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_INCREMENT, 2);
            pContext->offset += 2;
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_PLUS, 1);
            pContext->offset += 1;
        }
        return true;
    } 
	case '-': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_MINUS, 2);
            pContext->offset += 2;
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_DECREMENT, 2);
            pContext->offset += 2;
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_MINUS, 1);
            pContext->offset += 1;
        }
        return true;
    } 
	case '&': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_BITWISE_AND, 2);
            pContext->offset += 2;
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LOGICAL_AND, 2);
            pContext->offset += 2;
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_AND, 1);
            pContext->offset += 1;
        }
        return true;
    } 
	case '|': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_BITWISE_OR, 2);
            pContext->offset += 2;
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LOGICAL_OR, 2);
            pContext->offset += 2;
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_OR, 1);
            pContext->offset += 1;
        }
        return true;
    } 
	case '<': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LESS_OR_EQUAL, 2);
            pContext->offset += 2;
        } else if (secondSymbol == nextSymbol) {
            pContext->offset += 2;
            char thirdSymbol = ketl_lexer_get_symbol(pContext);
            pContext->offset -= 2;

            if (thirdSymbol == '=') {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_BITWISE_SHIFT_LEFT, 3);
                pContext->offset += 3;
            } else {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_SHIFT_LEFT, 2);
                pContext->offset += 2;
            }
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LESS, 1);
            pContext->offset += 1;
        }
        return true;
    } 
	case '>': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_GREATER_OR_EQUAL, 2);
            pContext->offset += 2;
        } else if (secondSymbol == nextSymbol) {
            pContext->offset += 2;
            char thirdSymbol = ketl_lexer_get_symbol(pContext);
            pContext->offset -= 2;

            if (thirdSymbol == '=') {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_BITWISE_SHIFT_RIGHT, 3);
                pContext->offset += 3;
            } else {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_SHIFT_RIGHT, 2);
                pContext->offset += 2;
            }
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_GREATER, 1);
            pContext->offset += 1;
        }
        return true;
    } 
    default:
        return false;
    }
}

ketl_token* ketl_lexer_build_tokens(const char* pSource, uint32_t length, uint32_t* pCount) {
    ketl_lexer_context context = {
        .pSource = pSource,
        .length = length,
        .offset = 0,
        .count = 0,
        .capacity = KETL_LEXER_INITIAL_TOKEN_CAPACITY,
        .pTokens = malloc(sizeof(ketl_token) * KETL_LEXER_INITIAL_TOKEN_CAPACITY),
        .lastTokenEnd = 0
    };

    KETL_FOREVER {
        char nextSymbol = ketl_lexer_get_symbol(&context);

        if (nextSymbol == '\0') {
            break;
        }

        if (ketl_lexer_is_space(nextSymbol)) {
            ++context.offset;
            continue;
        }

        if (ketl_lexer_parse_comments(&context, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_parse_literal_char(&context, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_parse_literal_string(&context, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_parse_literal_integer(&context, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_parse_id(&context, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_parse_operator(&context, nextSymbol)) {
            continue;
        }

        // TODO ERROR
        break;
    }

    *pCount = context.count;
    return context.pTokens;
}
