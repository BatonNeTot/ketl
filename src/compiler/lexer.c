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
    ketl_lexer_lines_t_push_back_copy(&p_lexer->lines, p_lexer->offset);
}

static void ketl_lexer_add_token(ketl_lexer_t* p_lexer, ketl_token_type type, uint32_t length) {
    ketl_lexer_tokens_t_push_back_copy(&p_lexer->tokens, (ketl_token_t){
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

static bool ketl_lexer_parse_next_line(ketl_lexer_t* p_lexer, char next_symbol) {
    if (next_symbol == '\r') {
        uint32_t offset = p_lexer->offset;
        p_lexer->offset += offset + 1 < p_lexer->length && p_lexer->p_source[offset + 1] == '\n' ? 2 : 1;
        ketl_lexer_increment_line(p_lexer);
        return true;
    }

    if (next_symbol == '\n') {
        p_lexer->offset += 1;
        ketl_lexer_increment_line(p_lexer);
        return true;
    }

    return false;
}

static bool ketl_lexer_parse_comments(ketl_lexer_t* p_lexer, char next_symbol) {
    uint32_t offset = p_lexer->offset;
    if (next_symbol != '/' || offset + 1 >= p_lexer->length) {
        return false;
    }

    next_symbol = p_lexer->p_source[offset + 1];

    // single line comment
    if (next_symbol == '/') {
        p_lexer->offset += 2;
        ANN_FOREVER {
            next_symbol = ketl_lexer_get_symbol(p_lexer);
            if (next_symbol == '\0' || ketl_lexer_parse_next_line(p_lexer, next_symbol)) {
                break;
            }
            p_lexer->offset += 1;
        }
        return true;
    }

    // multiline comment
    if (next_symbol == '*') {
        p_lexer->offset += 2;
        ANN_FOREVER {
            next_symbol = ketl_lexer_get_symbol(p_lexer);
            if (ketl_lexer_parse_next_line(p_lexer, next_symbol)) {
                continue;
            }

            if (next_symbol == '\0') {
                // TODO ERROR
                break;
            }

            if (next_symbol == '*' && p_lexer->offset + 1 < p_lexer->length && p_lexer->p_source[p_lexer->offset + 1] == '/') {
                p_lexer->offset += 2;
                break;
            }

            p_lexer->offset += 1;
        }

        return true;
    }

    return false;
}

static bool ketl_lexer_parse_literal_char(ketl_lexer_t* p_lexer, char next_symbol) {
    if (next_symbol != '\'') {
        return false;
    }

    uint32_t char_length = 1;
    uint32_t char_offset = p_lexer->offset += 1;

    if (ketl_lexer_get_symbol(p_lexer) == '\\') {
        ++char_length;
    }

    if (ketl_lexer_parse_next_line(p_lexer, next_symbol)) {
        // TODO ERROR and decide how to cleverly restore lexing
        ANN_ASSERT(false);
    }


    uint32_t end_mark_offset = char_offset + char_length;

    // error correction
    if (end_mark_offset >= p_lexer->length || p_lexer->p_source[end_mark_offset] != '\'') {
        // TODO ERROR
        if (end_mark_offset >= p_lexer->length) {
            char_length = p_lexer->length - char_offset;
        }
        ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LITERAL_CHAR, char_length);
        return true;
    }

    ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LITERAL_CHAR, char_length);
    p_lexer->offset += 1;
    return true;
}

static bool ketl_lexer_parse_literal_string(ketl_lexer_t* p_lexer, char next_symbol) {
    if (next_symbol != '"') {
        return false;
    }

    uint32_t literal_start_offset = p_lexer->offset += 1;
    ANN_FOREVER {
        next_symbol = ketl_lexer_get_symbol(p_lexer);
        if (ketl_lexer_parse_next_line(p_lexer, next_symbol)) {
            // TODO ERROR
            continue;
        }

        if (next_symbol == '\0') {
            // TODO ERROR
            break;
        }
        
        if (next_symbol == '"') {
            break;
        }

        p_lexer->offset += 1;
    }

    uint32_t literal_length = p_lexer->offset - literal_start_offset;
    p_lexer->offset = literal_start_offset;
    ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LITERAL_STRING, literal_length);
    p_lexer->offset += 1;
    return true;
}

static bool ketl_lexer_parse_literal_integer(ketl_lexer_t* p_lexer, char next_symbol) {
    if (!ketl_lexer_is_numeric(next_symbol)) {
        return false;
    }

    uint32_t literal_start_offset = p_lexer->offset;
    p_lexer->offset += 1;


    ANN_FOREVER {
        next_symbol = ketl_lexer_get_symbol(p_lexer);
        if (!ketl_lexer_is_numeric(next_symbol)) {
            break;
        }
        
        p_lexer->offset += 1;
    }

    uint32_t literal_length = p_lexer->offset - literal_start_offset;
    p_lexer->offset = literal_start_offset;
    ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LITERAL_INTEGER, literal_length);
    return true;
}

static bool ketl_lexer_parse_id(ketl_lexer_t* p_lexer, char next_symbol) {
    if (next_symbol != '_' && !ketl_lexer_is_alpha(next_symbol)) {
        return false;
    }

    uint32_t id_start_offset = p_lexer->offset;
    p_lexer->offset = id_start_offset + 1;

    ANN_FOREVER {
        next_symbol = ketl_lexer_get_symbol(p_lexer);
        if (next_symbol != '_' && !ketl_lexer_is_alpha(next_symbol) && !ketl_lexer_is_numeric(next_symbol)) {
            break;
        }
        
        p_lexer->offset += 1;
    }

    uint32_t id_length = p_lexer->offset - id_start_offset;
    p_lexer->offset = id_start_offset;

    char first_symbol = ketl_lexer_get_symbol(p_lexer);
    switch (first_symbol) {
        case 'c': {
            if (ketl_str_is_equal_n("class", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_CLASS, id_length);
                return true;
            }
            if (ketl_str_is_equal_n("cimport", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_CIMPORT, id_length);
                return true;
            }
            break;
        }
        case 'd': {
            if (ketl_str_is_equal_n("do", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_DO, id_length);
                return true;
            }
            break;
        }
        case 'e': {
            if (ketl_str_is_equal_n("else", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ELSE, id_length);
                return true;
            }
            if (ketl_str_is_equal_n("export", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_EXPORT, id_length);
                return true;
            }
            break;
        }
        case 'f': {
            if (ketl_str_is_equal_n("fn", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_FN, id_length);
                return true;
            }
            break;
        }
        case 'i': {
            if (ketl_str_is_equal_n("if", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_IF, id_length);
                return true;
            }
            if (ketl_str_is_equal_n("import", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_IMPORT, id_length);
                return true;
            }
            if (ketl_str_is_equal_n("i64", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_I64, id_length);
                return true;
            }
            break;
        }
        case 'r': {
            if (ketl_str_is_equal_n("raw", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_RAW, id_length);
                return true;
            }
            if (ketl_str_is_equal_n("return", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_RETURN, id_length);
                return true;
            }
            break;
        }
        case 'n': {
            if (ketl_str_is_equal_n("none", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_NONE, id_length);
                return true;
            }
            if (ketl_str_is_equal_n("null", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LITERAL_NULL, id_length);
                return true;
            }
            break;
        }
        case 'v': {
            if (ketl_str_is_equal_n("var", p_lexer->p_source + p_lexer->offset, id_length)) {
                ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_VAR, id_length);
                return true;
            }
            break;
        }
    }
    ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ID, id_length);
    return true;
}

static bool ketl_lexer_parse_operator(ketl_lexer_t* p_lexer, char next_symbol) {
    switch (next_symbol) {  
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
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;

        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_EQUAL, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN, 1);
        }
        return true;
    } 
	case '^': {
        p_lexer->offset += 1;
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_BITWISE_XOR, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_BITWISE_XOR, 1);
        }
        return true;
    } 
	case '*': {
        p_lexer->offset += 1;
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_MULTIPLY, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_MULTIPLY, 1);
        }
        return true;
    } 
	case '/': {
        p_lexer->offset += 1;
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_DIVIDE, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_DIVIDE, 1);
        }
        return true;
    } 
	case '%': {
        p_lexer->offset += 1;
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_REMAINDER, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_REMAINDER, 1);
        }
        return true;
    } 
	case '!': {
        p_lexer->offset += 1;
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_NOT_EQUAL, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LOGICAL_NOT, 1);
        }
        return true;
    } 
	case '+': {
        p_lexer->offset += 1;
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_PLUS, 2);
        } else if (second_symbol == '+') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_INCREMENT, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_PLUS, 1);
        }
        return true;
    } 
	case '-': {
        p_lexer->offset += 1;
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_MINUS, 2);
        } else if (second_symbol == '-') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_DECREMENT, 2);
        } else if (second_symbol == '>') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ARROW_RIGHT, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_MINUS, 1);
        }
        return true;
    } 
	case '&': {
        p_lexer->offset += 1;
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_BITWISE_AND, 2);
        } else if (second_symbol == next_symbol) {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LOGICAL_AND, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_BITWISE_AND, 1);
        }
        return true;
    } 
	case '|': {
        p_lexer->offset += 1;
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_ASSIGN_BITWISE_OR, 2);
        } else if (second_symbol == next_symbol) {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LOGICAL_OR, 2);
        } else {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_BITWISE_OR, 1);
        }
        return true;
    } 
	case '<': {
        p_lexer->offset += 1;
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_LESS_OR_EQUAL, 2);
        } else if (second_symbol == next_symbol) {
            p_lexer->offset += 2;
            char third_symbol = ketl_lexer_get_symbol(p_lexer);
            p_lexer->offset -= 2;

            if (third_symbol == '=') {
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
        char second_symbol = ketl_lexer_get_symbol(p_lexer);
        p_lexer->offset -= 1;
        
        if (second_symbol == '=') {
            ketl_lexer_add_token(p_lexer, KETL_TOKEN_TYPE_GREATER_OR_EQUAL, 2);
        } else if (second_symbol == next_symbol) {
            p_lexer->offset += 2;
            char third_symbol = ketl_lexer_get_symbol(p_lexer);
            p_lexer->offset -= 2;

            if (third_symbol == '=') {
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

void ketl_lexer_build_tokens(ketl_lexer_t* p_lexer, ketl_atomic_string s_filename, const char* p_source, uint32_t length) {
    p_lexer->s_filename = s_filename;
    p_lexer->p_source = p_source;
    p_lexer->length = length;
    p_lexer->offset = 0;
    p_lexer->token_iterator = 0;
    p_lexer->tokens.size = 0;
    p_lexer->lines.size = 0;
    ketl_lexer_lines_t_push_back_copy(&p_lexer->lines, 0);

    ANN_FOREVER {
        char next_symbol = ketl_lexer_get_symbol(p_lexer);

        if (next_symbol == '\0') {
            break;
        }

        if (ketl_lexer_parse_next_line(p_lexer, next_symbol)) {
            continue;
        }

        if (ketl_lexer_is_space(next_symbol)) {
            ++p_lexer->offset;
            continue;
        }

        if (ketl_lexer_parse_comments(p_lexer, next_symbol)) {
            continue;
        }

        if (ketl_lexer_parse_literal_char(p_lexer, next_symbol)) {
            continue;
        }

        if (ketl_lexer_parse_literal_string(p_lexer, next_symbol)) {
            continue;
        }

        if (ketl_lexer_parse_literal_integer(p_lexer, next_symbol)) {
            continue;
        }

        if (ketl_lexer_parse_id(p_lexer, next_symbol)) {
            continue;
        }

        if (ketl_lexer_parse_operator(p_lexer, next_symbol)) {
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
    ketl_lexer_tokens_t_init(&p_lexer->tokens, 4, p_allocator);
    ketl_lexer_lines_t_init(&p_lexer->lines, 4, p_allocator);
}

void ketl_lexer_deinit(ketl_lexer_t* p_lexer) {
    ketl_lexer_lines_t_deinit(&p_lexer->lines);
    ketl_lexer_tokens_t_deinit(&p_lexer->tokens);
}

uint32_t ketl_lexer_get_line_offset(ketl_lexer_t* p_lexer, uint32_t line) {
    if (line >= p_lexer->lines.size) {
        return p_lexer->length;
    }
    return p_lexer->lines.p_data[line];
}

uint32_t ketl_lexer_find_line(ketl_lexer_t* p_lexer, uint32_t offset) {
    uint32_t lhs = 0, rhs = p_lexer->lines.size;

    while (lhs + 1 < rhs) {
        uint32_t mid = (lhs + rhs) / 2;
        uint32_t mid_offset = p_lexer->lines.p_data[mid];
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
