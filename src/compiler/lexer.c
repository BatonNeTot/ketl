//🫖ketl
#include "lexer.h"

#include "memory_impl.h"
#include "str.h"

#include <string.h>


KETL_VECTOR_DEFINITION(ketl_lexer_tokens_t, ketl_token_t)
KETL_VECTOR_DEFINITION(ketl_lexer_lines_t, uint32_t)

static inline bool ketl_lexer_is_space(char symbol) {
    return symbol == ' ' || (symbol >= '\t' && symbol <= '\r');
}

static inline bool ketl_lexer_is_numeric(char symbol) {
    return symbol >= '0' && symbol <= '9';
}

static inline bool ketl_lexer_is_alpha(char symbol) {
    return (symbol >= 'a' && symbol <= 'z') || (symbol >= 'A' && symbol <= 'Z');
}

static void ketl_lexer_increment_line(ketl_lexer_t* p_lexer) {
    ketl_lexer_lines_t_push_back_copy(&p_lexer->v_lines, p_lexer->offset);
}

static void ketl_lexer_add_token(ketl_lexer_t* p_lexer, ketl_token_type type, uint32_t length) {
    ketl_lexer_tokens_t_push_back_copy(&p_lexer->v_tokens, (ketl_token_t){
        .type = type, 
        .length = (ketl_token_length_t)length, 
        .offset = p_lexer->offset,
    });
    p_lexer->offset += length;
}

static char ketl_lexer_get_symbol(ketl_lexer_t* p_lexer) {
    char symbol = '\0';
    uint32_t offset = p_lexer->offset;
    if (offset < p_lexer->length) {
        symbol = p_lexer->p_source[offset];
    }
    return symbol;
}

static bool ketl_lexer_parse_next_line(ketl_lexer_t* p_lexer, char nextSymbol) {
    if (nextSymbol == '\r') {
        uint32_t offset = p_lexer->offset;
        p_lexer->offset += offset + 1 < p_lexer->length && p_lexer->p_source[offset + 1] == '\n' ? 2 : 1;
        ketl_lexer_increment_line(p_lexer);
        return true;
    }

    if (nextSymbol == '\n') {
        p_lexer->offset += 1;
        ketl_lexer_increment_line(p_lexer);
        return true;
    }

    return false;
}

static bool ketl_lexer_parse_comments(ketl_lexer_t* p_lexer, char nextSymbol) {
    uint32_t offset = p_lexer->offset;
    if (nextSymbol != '/' || offset + 1 >= p_lexer->length) {
        return false;
    }

    nextSymbol = p_lexer->p_source[offset + 1];

    // single line comment
    if (nextSymbol == '/') {
        p_lexer->offset += 2;
        ANN_FOREVER {
            nextSymbol = ketl_lexer_get_symbol(p_lexer);
            if (nextSymbol == '\0' || ketl_lexer_parse_next_line(p_lexer, nextSymbol)) {
                break;
            }
            p_lexer->offset += 1;
        }
        return true;
    }

    // multiline comment
    if (nextSymbol == '*') {
        p_lexer->offset += 2;
        ANN_FOREVER {
            nextSymbol = ketl_lexer_get_symbol(p_lexer);
            if (ketl_lexer_parse_next_line(p_lexer, nextSymbol)) {
                continue;
            }

            if (nextSymbol == '\0') {
                // TODO ERROR
                break;
            }

            if (nextSymbol == '*' && p_lexer->offset + 1 < p_lexer->length && p_lexer->p_source[p_lexer->offset + 1] == '/') {
                p_lexer->offset += 2;
                break;
            }

            p_lexer->offset += 1;
        }

        return true;
    }

    return false;
}

static bool ketl_lexer_parse_literal_char(ketl_lexer_t* p_lexer, char nextSymbol) {
    if (nextSymbol != '\'') {
        return false;
    }

    uint32_t charLength = 1;
    uint32_t charOffset = p_lexer->offset += 1;

    if (ketl_lexer_get_symbol(p_lexer) == '\\') {
        ++charLength;
    }

    if (ketl_lexer_parse_next_line(p_lexer, nextSymbol)) {
        // TODO ERROR and decide how to cleverly restore lexing
        ANN_ASSERT(false);
    }


    uint32_t endMarkOffset = charOffset + charLength;

    // error correction
    if (endMarkOffset >= p_lexer->length || p_lexer->p_source[endMarkOffset] != '\'') {
        // TODO ERROR
        if (endMarkOffset >= p_lexer->length) {
            charLength = p_lexer->length - charOffset;
        }
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LITERAL_CHAR, charLength);
        return true;
    }

    ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LITERAL_CHAR, charLength);
    p_lexer->offset += 1;
    return true;
}

static bool ketl_lexer_parse_literal_string(ketl_lexer_t* p_lexer, char nextSymbol) {
    if (nextSymbol != '"') {
        return false;
    }

    uint32_t literalStartOffset = p_lexer->offset += 1;
    ANN_FOREVER {
        nextSymbol = ketl_lexer_get_symbol(p_lexer);
        if (ketl_lexer_parse_next_line(p_lexer, nextSymbol)) {
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

        p_lexer->offset += 1;
    }

    uint32_t literalLength = p_lexer->offset - literalStartOffset;
    p_lexer->offset = literalStartOffset;
    ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LITERAL_STRING, literalLength);
    p_lexer->offset += 1;
    return true;
}

static bool ketl_lexer_parse_literal_integer(ketl_lexer_t* p_lexer, char nextSymbol) {
    if (!ketl_lexer_is_numeric(nextSymbol)) {
        return false;
    }

    uint32_t literalStartOffset = p_lexer->offset;
    p_lexer->offset += 1;


    ANN_FOREVER {
        nextSymbol = ketl_lexer_get_symbol(p_lexer);
        if (!ketl_lexer_is_numeric(nextSymbol)) {
            break;
        }
        
        p_lexer->offset += 1;
    }

    uint32_t literalLength = p_lexer->offset - literalStartOffset;
    p_lexer->offset = literalStartOffset;
    ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LITERAL_INTEGER, literalLength);
    return true;
}

static bool ketl_lexer_parse_id(ketl_lexer_t* p_lexer, char nextSymbol) {
    if (nextSymbol != '_' && !ketl_lexer_is_alpha(nextSymbol)) {
        return false;
    }

    uint32_t idStartOffset = p_lexer->offset;
    p_lexer->offset = idStartOffset + 1;

    ANN_FOREVER {
        nextSymbol = ketl_lexer_get_symbol(p_lexer);
        if (nextSymbol != '_' && !ketl_lexer_is_alpha(nextSymbol) && !ketl_lexer_is_numeric(nextSymbol)) {
            break;
        }
        
        p_lexer->offset += 1;
    }

    uint32_t idLength = p_lexer->offset - idStartOffset;
    p_lexer->offset = idStartOffset;

    char firstSymbol = ketl_lexer_get_symbol(p_lexer);
    switch (firstSymbol) {
        case 'd': {
            if (ketl_str_is_equal_n("do", p_lexer->p_source + p_lexer->offset, idLength)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_DO, idLength);
                return true;
            }
            break;
        }
        case 'e': {
            if (ketl_str_is_equal_n("else", p_lexer->p_source + p_lexer->offset, idLength)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ELSE, idLength);
                return true;
            }
            break;
        }
        case 'i': {
            if (ketl_str_is_equal_n("if", p_lexer->p_source + p_lexer->offset, idLength)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_IF, idLength);
                return true;
            }
            if (ketl_str_is_equal_n("i64", p_lexer->p_source + p_lexer->offset, idLength)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_I64, idLength);
                return true;
            }
            break;
        }
        case 'r': {
            if (ketl_str_is_equal_n("return", p_lexer->p_source + p_lexer->offset, idLength)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_RETURN, idLength);
                return true;
            }
            break;
        }
        case 'v': {
            if (ketl_str_is_equal_n("var", p_lexer->p_source + p_lexer->offset, idLength)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_VAR, idLength);
                return true;
            }
            break;
        }
    }
    ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ID, idLength);
    return true;
}

static bool ketl_lexer_parse_operator(ketl_lexer_t* p_lexer, char nextSymbol) {
    switch (nextSymbol) {  
    case '(': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, 1);
        return true;
    } 
	case ')': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, 1);
        return true;
    } 
	case '{': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_CURLY_LEFT, 1);
        return true;
    } 
	case '}': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_CURLY_RIGHT, 1);
        return true;
    } 
	case '[': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_SQUARE_LEFT, 1);
        return true;
    } 
	case ']': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_SQUARE_RIGHT, 1);
        return true;
    } 
	case '.': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_DOT, 1);
        return true;
    } 
	case ',': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_COMMA, 1);
        return true;
    } 
	case '?': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_QUESTION_MARK, 1);
        return true;
    } 
	case ':': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_COLON, 1);
        return true;
    } 
	case ';': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, 1);
        return true;
    } 
	case '~': {
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_BITWISE_NOT, 1);
        return true;
    } 
	case '=': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;

        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_EQUAL, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN, 1);
        }
        return true;
    } 
	case '^': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_BITWISE_XOR, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_BITWISE_XOR, 1);
        }
        return true;
    } 
	case '*': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_MULTIPLY, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_MULTIPLY, 1);
        }
        return true;
    } 
	case '/': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_DIVIDE, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_DIVIDE, 1);
        }
        return true;
    } 
	case '%': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_REMAINDER, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_REMAINDER, 1);
        }
        return true;
    } 
	case '!': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_NOT_EQUAL, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LOGICAL_NOT, 1);
        }
        return true;
    } 
	case '+': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_PLUS, 2);
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_INCREMENT, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_PLUS, 1);
        }
        return true;
    } 
	case '-': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_MINUS, 2);
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_DECREMENT, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_MINUS, 1);
        }
        return true;
    } 
	case '&': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_BITWISE_AND, 2);
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LOGICAL_AND, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_BITWISE_AND, 1);
        }
        return true;
    } 
	case '|': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_BITWISE_OR, 2);
        } else if (secondSymbol == nextSymbol) {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LOGICAL_OR, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_BITWISE_OR, 1);
        }
        return true;
    } 
	case '<': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LESS_OR_EQUAL, 2);
        } else if (secondSymbol == nextSymbol) {
            p_lexer->offset += 2;
            char thirdSymbol = ketl_lexer_get_symbol(p_lexer);
            p_lexer->offset -= 2;

            if (thirdSymbol == '=') {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_BITWISE_SHIFT_LEFT, 3);
            } else {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_BITWISE_SHIFT_LEFT, 2);
            }
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LESS, 1);
        }
        return true;
    } 
	case '>': {
        p_lexer->offset += 1;
        char secondSymbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (secondSymbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_GREATER_OR_EQUAL, 2);
        } else if (secondSymbol == nextSymbol) {
            p_lexer->offset += 2;
            char thirdSymbol = ketl_lexer_get_symbol(p_lexer);
            p_lexer->offset -= 2;

            if (thirdSymbol == '=') {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_BITWISE_SHIFT_RIGHT, 3);
            } else {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_BITWISE_SHIFT_RIGHT, 2);
            }
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_GREATER, 1);
        }
        return true;
    } 
    default:
        return false;
    }
}

void ketl_lexer_build_tokens(ketl_lexer_t* p_lexer, const char* p_source, uint32_t length) {
    p_lexer->p_source = p_source;
    p_lexer->length = length;
    p_lexer->offset = 0;
    p_lexer->v_tokens.size = 0;
    p_lexer->v_lines.size = 0;
    ketl_lexer_lines_t_push_back_copy(&p_lexer->v_lines, 0);

    ANN_FOREVER {
        char nextSymbol = ketl_lexer_get_symbol(p_lexer);

        if (nextSymbol == '\0') {
            break;
        }

        if (ketl_lexer_parse_next_line(p_lexer, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_is_space(nextSymbol)) {
            ++p_lexer->offset;
            continue;
        }

        if (ketl_lexer_parse_comments(p_lexer, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_parse_literal_char(p_lexer, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_parse_literal_string(p_lexer, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_parse_literal_integer(p_lexer, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_parse_id(p_lexer, nextSymbol)) {
            continue;
        }

        if (ketl_lexer_parse_operator(p_lexer, nextSymbol)) {
            continue;
        }

        // TODO ERROR
        break;
    }

    ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_EOF, 0);
}

void ketl_lexer_init(ketl_lexer_t* p_lexer, const ketl_allocator* p_allocator) {
    *p_lexer = (ketl_lexer_t){0};
    *p_lexer = (ketl_lexer_t){
        .p_allocator = p_allocator,
    };
    ketl_lexer_tokens_t_init(&p_lexer->v_tokens, 4, p_allocator);
    ketl_lexer_lines_t_init(&p_lexer->v_lines, 4, p_allocator);
}

void ketl_lexer_deinit(ketl_lexer_t* p_lexer) {
    ketl_lexer_lines_t_deinit(&p_lexer->v_lines);
    ketl_lexer_tokens_t_deinit(&p_lexer->v_tokens);
}

uint32_t ketl_lexer_get_line_offset(ketl_lexer_t* p_lexer, uint32_t line) {
    if (line >= p_lexer->v_lines.size) {
        return p_lexer->length;
    }
    return p_lexer->v_lines.p_data[line];
}

uint32_t ketl_lexer_find_line(ketl_lexer_t* p_lexer, uint32_t offset) {
    uint32_t lhs = 0, rhs = p_lexer->v_lines.size;

    while (lhs + 1 < rhs) {
        uint32_t mid = (lhs + rhs) / 2;
        uint32_t mid_offset = p_lexer->v_lines.p_data[mid];
        if (mid_offset < offset) {
            lhs = mid;
        } else if (offset < mid_offset) {
            rhs = mid;
        } else {
            return mid;
        }
    }
    return lhs;
}
