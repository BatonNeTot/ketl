//🫖ketl
#include "ketl/utils.h"

#if ANN_OS_WINDOWS
#include "assembler/x86_win32.cxx"
#else
#include "assembler/x86_system_v.cxx"
#endif

#include <stdio.h>

uint32_t ketl_assembler_format(uint8_t* pOpcodes, uint32_t opcodesSize, char* buffer, uint32_t bufferSize) {
    uint32_t printedCount = 0u;
    for (uint64_t i = 0; i < opcodesSize; ++i) {
        if (i != 0 && (i & 15) == 0) {
            printedCount += snprintf(buffer + printedCount, bufferSize - printedCount, "\n");
        }
        printedCount += snprintf(buffer + printedCount, bufferSize - printedCount, "%.2X ", pOpcodes[i]);
    }
    return printedCount;
}
