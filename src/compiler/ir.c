//🫖ketl
#include "compiler/ir.h"

#include <stdio.h>

uint32_t ketl_ir_node_format(ketl_ir_node node, const char* pSymbols, char* buffer, uint32_t bufferSize) {
    switch(node.type) {
        case KETL_IR_TYPE_PUSH_ARGUMENT: {
            return snprintf(buffer, bufferSize, "push argument %s", 
            pSymbols + node.aArgs[0]);
        }
        case KETL_IR_TYPE_CALL: {
            return snprintf(buffer, bufferSize, "%s = call %s", 
            pSymbols + node.aArgs[0], pSymbols + node.aArgs[1]);
        }
        case KETL_IR_TYPE_RETURN: {
            return snprintf(buffer, bufferSize, "return");
        }
        case KETL_IR_TYPE_RETURN_VALUE: {
            return snprintf(buffer, bufferSize, "return %s", 
            pSymbols + node.aArgs[0]);
        }
        case KETL_IR_TYPE_PLUS: {
            return snprintf(buffer, bufferSize, "%s = %s + %s", 
            pSymbols + node.aArgs[0], pSymbols + node.aArgs[1], pSymbols + node.aArgs[2]);
        }
        case KETL_IR_TYPE_MULTIPLY: {
            return snprintf(buffer, bufferSize, "%s = %s * %s", 
            pSymbols + node.aArgs[0], pSymbols + node.aArgs[1], pSymbols + node.aArgs[2]);
        }
        default: {
            return snprintf(buffer, bufferSize, "can't format ir %d (%s, %s, %s)", 
            node.type, pSymbols + node.aArgs[0], pSymbols + node.aArgs[1], pSymbols + node.aArgs[2]);
        }
    }
}
