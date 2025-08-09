//🫖ketl
#include "lexer.h"

#include "memory_impl.h"
#include "str.h"

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
    const ketl_allocator* pAllocator;
    const char* pSource;
    uint32_t length;
    uint32_t offset;
    uint32_t line;
    uint16_t col;
    uint32_t count;
    uint32_t capacity;
    ketl_token* pTokens;
    uint32_t lastTokenEnd;
};

static void ketl_lexer_increment_line(ketl_lexer_context* p_context) {
    ++p_context->line;
    p_context->col = 0;
}

static void ketl_lexer_add_token(ketl_lexer_context* pContext, ketl_token_type type, uint32_t length) {
    uint32_t count = pContext->count;
    if (count >= pContext->capacity) {
        uint32_t newCapacity = (uint32_t)(pContext->capacity << 1);
        KETL_ASSERT(newCapacity > pContext->capacity);
        pContext->capacity = newCapacity;
        pContext->pTokens = ketl_realloc(pContext->pAllocator, pContext->pTokens, sizeof(ketl_token) * newCapacity);
    }
    uint32_t offset = pContext->offset;
    uint32_t prevOffset = offset - pContext->lastTokenEnd;
    pContext->pTokens[count] = (ketl_token){
        .type = type, 
        .length = length, 
        .prevOffset = prevOffset,
        .start_pos_line = pContext->line,
        .end_pos_line = pContext->line,
        .start_pos_col = pContext->col,
        .end_pos_col = (pContext->col += length),
    };
    ++pContext->count;
    pContext->lastTokenEnd = offset + length;
    pContext->offset += length;
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
        ketl_lexer_increment_line(pContext);
        return true;
    }

    if (nextSymbol == '\n') {
        pContext->offset += 1;
        ketl_lexer_increment_line(pContext);
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
        pContext->col += 2;
        KETL_FOREVER {
            nextSymbol = ketl_lexer_get_symbol(pContext);
            if (nextSymbol == '\0' || ketl_lexer_parse_next_line(pContext, nextSymbol)) {
                break;
            }
            pContext->offset += 1;
            pContext->col += 1;
        }
        return true;
    }

    // multiline comment
    if (nextSymbol == '*') {
        pContext->offset += 2;
        pContext->col += 2;
        KETL_FOREVER {
            nextSymbol = ketl_lexer_get_symbol(pContext);
            if (ketl_lexer_parse_next_line(pContext, nextSymbol)) {
                continue;
            }

            if (nextSymbol == '\0') {
                // TODO ERROR
                break;
            }

            if (nextSymbol == '*' && pContext->offset + 1 < pContext->length && pContext->pSource[pContext->offset + 1] == '/') {
                pContext->offset += 2;
                pContext->col += 2;
                break;
            }

            pContext->offset += 1;
            pContext->col += 1;
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
    pContext->col += 1;

    if (ketl_lexer_get_symbol(pContext) == '\\') {
        ++charLength;
    }

    if (ketl_lexer_parse_next_line(pContext, nextSymbol)) {
        // TODO ERROR and decide how to cleverly restore lexing
        KETL_ASSERT(false);
    }


    uint32_t endMarkOffset = charOffset + charLength;

    // error correction
    if (endMarkOffset >= pContext->length || pContext->pSource[endMarkOffset] != '\'') {
        // TODO ERROR
        if (endMarkOffset >= pContext->length) {
            charLength = pContext->length - charOffset;
        }
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LITERAL_CHAR, charLength);
        return true;
    }

    ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LITERAL_CHAR, charLength);
    pContext->offset += 1;
    pContext->col += 1;
    return true;
}

static bool ketl_lexer_parse_literal_string(ketl_lexer_context* pContext, char nextSymbol) {
    if (nextSymbol != '"') {
        return false;
    }

    uint32_t literalStartOffset = pContext->offset += 1;
    pContext->col += 1;
    KETL_FOREVER {
        nextSymbol = ketl_lexer_get_symbol(pContext);
        if (ketl_lexer_parse_next_line(pContext, nextSymbol)) {
            // TODO ERROR
            continue;
        }

        if (nextSymbol == '\0') {
            // TODO ERROR
            break;
        }
        
        if (nextSymbol == '"') {
            break;
        }

        pContext->offset += 1;
    }

    uint32_t literalLength = pContext->offset - literalStartOffset;
    pContext->offset = literalStartOffset;
    ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LITERAL_STRING, literalLength);
    pContext->offset += 1;
    pContext->col += 1;
    return true;
}

static bool ketl_lexer_parse_literal_integer(ketl_lexer_context* pContext, char nextSymbol) {
    if (!ketl_lexer_is_numeric(nextSymbol)) {
        return false;
    }

    uint32_t literalStartOffset = pContext->offset;
    pContext->offset += 1;


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
    return true;
}

static bool ketl_lexer_parse_id(ketl_lexer_context* pContext, char nextSymbol) {
    if (nextSymbol != '_' && !ketl_lexer_is_alpha(nextSymbol)) {
        return false;
    }

    uint32_t idStartOffset = pContext->offset;
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

    char firstSymbol = ketl_lexer_get_symbol(pContext);
    switch (firstSymbol) {
        case 'e': {
            if (ketl_str_is_equal_n("else", pContext->pSource + pContext->offset, idLength)) {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ELSE, idLength);
                return true;
            }
            break;
        }
        case 'i': {
            if (ketl_str_is_equal_n("if", pContext->pSource + pContext->offset, idLength)) {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_IF, idLength);
                return true;
            }
            if (ketl_str_is_equal_n("i64", pContext->pSource + pContext->offset, idLength)) {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_I64, idLength);
                return true;
            }
            break;
        }
        case 'r': {
            if (ketl_str_is_equal_n("return", pContext->pSource + pContext->offset, idLength)) {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_RETURN, idLength);
                return true;
            }
            break;
        }
    }
    ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ID, idLength);
    return true;
}

static bool ketl_lexer_parse_operator(ketl_lexer_context* pContext, char nextSymbol) {
    switch (nextSymbol) {  
    case '(': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, 1);
        return true;
    } 
	case ')': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, 1);
        return true;
    } 
	case '{': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_CURLY_LEFT, 1);
        return true;
    } 
	case '}': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_CURLY_RIGHT, 1);
        return true;
    } 
	case '[': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_SQUARE_LEFT, 1);
        return true;
    } 
	case ']': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_SQUARE_RIGHT, 1);
        return true;
    } 
	case '.': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_DOT, 1);
        return true;
    } 
	case ',': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_COMMA, 1);
        return true;
    } 
	case '?': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_TERNARY_FIRST, 1);
        return true;
    } 
	case ':': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_TERNARY_SECOND, 1);
        return true;
    } 
	case ';': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, 1);
        return true;
    } 
	case '~': {
        ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_NOT, 1);
        return true;
    } 
	case '=': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;

        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_EQUAL, 2);
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN, 1);
        }
        return true;
    } 
	case '^': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_BITWISE_XOR, 2);
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_XOR, 1);
        }
        return true;
    } 
	case '*': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_MULTIPLY, 2);
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_MULTIPLY, 1);
        }
        return true;
    } 
	case '/': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_DIVIDE, 2);
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_DIVIDE, 1);
        }
        return true;
    } 
	case '%': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_REMAINDER, 2);
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_REMAINDER, 1);
        }
        return true;
    } 
	case '!': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_NOT_EQUAL, 2);
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LOGICAL_NOT, 1);
        }
        return true;
    } 
	case '+': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_PLUS, 2);
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_INCREMENT, 2);
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_PLUS, 1);
        }
        return true;
    } 
	case '-': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_MINUS, 2);
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_DECREMENT, 2);
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_MINUS, 1);
        }
        return true;
    } 
	case '&': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_BITWISE_AND, 2);
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LOGICAL_AND, 2);
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_AND, 1);
        }
        return true;
    } 
	case '|': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_BITWISE_OR, 2);
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LOGICAL_OR, 2);
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_OR, 1);
        }
        return true;
    } 
	case '<': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LESS_OR_EQUAL, 2);
        } else if (secondSymbol == nextSymbol) {
            pContext->offset += 2;
            char thirdSymbol = ketl_lexer_get_symbol(pContext);
            pContext->offset -= 2;

            if (thirdSymbol == '=') {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_BITWISE_SHIFT_LEFT, 3);
            } else {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_SHIFT_LEFT, 2);
            }
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_LESS, 1);
        }
        return true;
    } 
	case '>': {
        pContext->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(pContext);
        pContext->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_GREATER_OR_EQUAL, 2);
        } else if (secondSymbol == nextSymbol) {
            pContext->offset += 2;
            char thirdSymbol = ketl_lexer_get_symbol(pContext);
            pContext->offset -= 2;

            if (thirdSymbol == '=') {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_ASSIGN_BITWISE_SHIFT_RIGHT, 3);
            } else {
                ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_BITWISE_SHIFT_RIGHT, 2);
            }
        } else {
            ketl_lexer_add_token(pContext, KETL_TOKEN_TYPE_GREATER, 1);
        }
        return true;
    } 
    default:
        return false;
    }
}

ketl_token* ketl_lexer_build_tokens(const char* pSource, uint32_t length, uint32_t* pCount, const ketl_allocator* pAllocator) {
    ketl_lexer_context context = {
        .pAllocator = pAllocator,
        .pSource = pSource,
        .length = length,
        .offset = 0,
        .line = 0,
        .col = 0,
        .count = 0,
        .capacity = KETL_LEXER_INITIAL_TOKEN_CAPACITY,
        .pTokens = ketl_alloc(pAllocator, sizeof(ketl_token) * KETL_LEXER_INITIAL_TOKEN_CAPACITY),
        .lastTokenEnd = 0
    };

    KETL_FOREVER {
        char nextSymbol = ketl_lexer_get_symbol(&context);

        if (nextSymbol == '\0') {
            break;
        }

        if (ketl_lexer_parse_next_line(&context, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_is_space(nextSymbol)) {
            ++context.offset;
            ++context.col;
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
