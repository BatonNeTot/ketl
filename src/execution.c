//🫖ketl
#include "execution.h"

#include "ketl/utils.h"

/*
__asm__(".globl ketl_execute\n\t"
#if !KETL_OS_WINDOWS
        ".type ketl_execute, @function\n\t"
#endif
        "ketl_execute:\n\t"
        ".cfi_startproc\n\t"
#if !KETL_OS_WINDOWS
        "jmp *(%rdi)\n\t" // rdi == functionClass*
#else
        "jmp *(%rcx)\n\t" // rcx == functionClass*
#endif
        ".cfi_endproc");
*/
