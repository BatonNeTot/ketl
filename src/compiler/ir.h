//🫖ketl
#ifndef ketl_compiler_ir_h
#define ketl_compiler_ir_h

#include "ketl/utils.h"

typedef uint8_t ketl_ir_type;

enum __KETL_IR_TYPE {
    KETL_IR_TYPE_RETURN,
    KETL_IR_TYPE_RETURN_VALUE,

    KETL_IR_TYPE_ASSIGN,

    KETL_IR_TYPE_PLUS,
    KETL_IR_TYPE_MULTIPLY,

    // only count binary operators for now
    KETL_IR_FIRST_OPERATOR = KETL_IR_TYPE_PLUS,
    KETL_IR_LAST_OPERATOR = KETL_IR_TYPE_MULTIPLY,
};

KETL_DEFINE(ketl_ir_node) {
    ketl_ir_type type;
    uint16_t aArgs[3];
};

KETL_DEFINE(ketl_ir) {
    ketl_ir_node* pNodes;
    char* pSymbols;
    uint32_t nodesCount;
};

uint32_t ketl_ir_node_format(ketl_ir_node node, const char* pSymbols, char* buffer, uint32_t bufferSize);

#endif // ketl_compiler_ir_h
