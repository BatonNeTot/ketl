//🫖ketl
#include "compiler/ir.h"

#include <stdio.h>

uint32_t ketl_ir_node_format(ketl_ir_node node, const char* pSymbols, char* buffer, uint32_t bufferSize) {
    switch(node.type) {
        case KETL_IR_TYPE_ASSIGN: {

            return 0;
        }
        case KETL_IR_TYPE_PLUS: {
            return sprintf_s(buffer, bufferSize, "%s = %s + %s", 
            pSymbols + node.args[2], pSymbols + node.args[0], pSymbols + node.args[1]);
        }
        case KETL_IR_TYPE_MULTIPLY: {
            return sprintf_s(buffer, bufferSize, "%s = %s * %s", 
            pSymbols + node.args[2], pSymbols + node.args[0], pSymbols + node.args[1]);
        }
    }
    return 0;
}