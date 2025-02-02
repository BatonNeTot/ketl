//🫖ketl
#include "compiler/ir.h"

#include <stdio.h>

uint32_t ketl_ir_node_format(ketl_ir_node node, const char* pSymbols, char* buffer, uint32_t bufferSize) {
    switch(node.type) {
        case KETL_IR_TYPE_RETURN: {
            return snprintf(buffer, bufferSize, "return");
        }
        case KETL_IR_TYPE_RETURN_VALUE: {
            return snprintf(buffer, bufferSize, "return %s", 
            pSymbols + node.args[0]);
        }
        case KETL_IR_TYPE_PLUS: {
            return snprintf(buffer, bufferSize, "%s = %s + %s", 
            pSymbols + node.args[0], pSymbols + node.args[1], pSymbols + node.args[2]);
        }
        case KETL_IR_TYPE_MULTIPLY: {
            return snprintf(buffer, bufferSize, "%s = %s * %s", 
            pSymbols + node.args[0], pSymbols + node.args[1], pSymbols + node.args[2]);
        }
        default: {
            return snprintf(buffer, bufferSize, "can't format ir %d (%s, %s, %s)", 
            node.type, pSymbols + node.args[0], pSymbols + node.args[1], pSymbols + node.args[2]);
        }
    }
}