//🫖ketl
#include "error_stream.h"

#include "compiler/lexer.h"
#include "ketl_impl.h"

#include <stdio.h>
#include <stdarg.h>

char error_buffer[16384];

void ketl_error_report(ketl_state* p_state, ketl_error_info* p_error_info, const char* format, ...) {
    uint32_t start_line, start_col;
    start_line = ketl_lexer_find_line(p_error_info->p_lexer, p_error_info->offset);
    start_col = p_error_info->offset - ketl_lexer_get_line_offset(p_error_info->p_lexer, start_line);

    int message_size = 0;
    message_size += snprintf(error_buffer + message_size, ANN_ARRAY_SIZE(error_buffer) - message_size, 
        "%s:%"PRIu32":%"PRIu32": error: ", ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_error_info->s_filename), start_line + 1, start_col + 1);

    va_list vargs;
    va_start(vargs, format);
    message_size += vsnprintf(error_buffer + message_size, ANN_ARRAY_SIZE(error_buffer) - message_size, format, vargs);
    va_end(vargs);

    uint32_t end_offset = p_error_info->offset + p_error_info->length + 1;
    uint32_t end_line = ketl_lexer_find_line(p_error_info->p_lexer, end_offset - 1) + 1;

    for (uint32_t line = start_line; line != end_line; ++line) {
        uint32_t start_line_offset = ketl_lexer_get_line_offset(p_error_info->p_lexer, line);
        uint32_t end_line_offset = ketl_lexer_get_line_offset(p_error_info->p_lexer, line + 1);

        // TODO check for all new line configs
        if (end_line_offset > 0 && p_error_info->p_lexer->p_source[end_line_offset - 1] == '\n') {
            --end_line_offset;
        }
        message_size += snprintf(error_buffer + message_size, ANN_ARRAY_SIZE(error_buffer) - message_size, 
            "\n %4d |%.*s", line + 1, end_line_offset - start_line_offset, p_error_info->p_lexer->p_source + start_line_offset);

        message_size += snprintf(error_buffer + message_size, ANN_ARRAY_SIZE(error_buffer) - message_size, 
            "\n      |");
            
        uint32_t select_start = start_line_offset;
        uint32_t select_end = end_offset;

        if (line == start_line) {
            message_size += snprintf(error_buffer + message_size, ANN_ARRAY_SIZE(error_buffer) - message_size, 
                "%*s^", start_col, "");
            select_start = start_line_offset + 1 + start_col;
        }
        
        if (select_end > end_line_offset) {
            select_end = end_line_offset;
        }

        if (select_start <= select_end) {
            for (uint32_t i = select_start; i != select_end; ++i) {
                message_size += snprintf(error_buffer + message_size, ANN_ARRAY_SIZE(error_buffer) - message_size, 
                "~");
            }
        }
    }
    
    //fprintf(stderr, "ERROR SIZE %d\n", message_size);
    //error_buffer[message_size] = '\0';
    //fputs(error_buffer, stderr);

    string_builder_t_push_back_ref_n(&p_state->error_stream, error_buffer, message_size);
    string_builder_t_push_back_copy(&p_state->error_stream, '\n');
}
