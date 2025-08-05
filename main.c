#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint64_t sp;        // Stack Pointer
    uint64_t fp;        // Frame Pointer (x29)
    uint64_t lr;        // Link Register (x30)
} stack_info_t;

void get_stack_info(stack_info_t* info) {
    __asm volatile(
        "mov x1, sp\n"          
        "str x1, [%0, #0]\n"    // info->sp = x1 (sp)
        "str x29, [%0, #8]\n"   // info->fp = fp
        "str x30, [%0, #16]\n"  // info->lr = lr
        :: "r"(info) : "x1", "memory"
    );
}

void print_stack_info(stack_info_t info) {
    printf("SP (Stack Pointer): 0x%llx\n", info.sp);
    printf("FP (Frame Pointer): 0x%llx\n", info.fp);
    printf("LR (Link Register): 0x%llx\n", info.lr);
}

int main() {
    stack_info_t info; 
    get_stack_info(&info);
    print_stack_info(info);
    return 0;
}

