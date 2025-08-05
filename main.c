#include <stdio.h>

long __attribute__((naked)) foo() {
    __asm("mov x5, #10\n\t"
        "mov x0, x5\n\t"
        "ret");
}

int main() {
    long tmp = foo();
    printf("%ld\n", tmp);
    return 0;
}

