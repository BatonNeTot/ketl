#include "ketl/utils.h"

typedef uint8_t ketl_token_type;



KETL_DEFINE(ketl_token) {
    uint32_t code;
};

const ketl_token* ketl_lexer_build_tokens(const char* source, uint64_t length, uint32_t* count);

ketl_token ketl_lexer_token_create_token();

inline uint32_t ketl_lexer_get_token_length(const char* source, ketl_token token) {
    (void)source;
    (void)token;

}
