#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h> // mmap here 
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

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

void* get_top_of_main_stack() {
    pthread_t self = pthread_self();
    return pthread_get_stackaddr_np(self);
}

size_t get_main_stack_size() {
    pthread_t self = pthread_self();
    return pthread_get_stacksize_np(self);
}

//-------------------------------------------------------------------------

typedef struct {
    void* sp;
    void* top_of_stack;
    void* stack_base;
    bool is_main;
    bool is_done; // TODO: remove this fild
} context_t;

#define MIN_STACK_SIZE 1024 * 4 //TODO: chacnge "4" on getpagesize() func

#define CONTEXT_ARR_CAPACITY 3
static context_t g_ctxs[CONTEXT_ARR_CAPACITY] = {0};
static size_t current_ctx = 0;

void set_to_ctxs(void* sp, void* top_of_stack, size_t index) {
    if (CONTEXT_ARR_CAPACITY - 1 < index) {
        printf("can't set context , index was outside ret\n");
        return;
    }

    g_ctxs[index].sp = sp;
    g_ctxs[index].top_of_stack = top_of_stack;
}

//-------------------------------------------------------------------------

void*  __attribute__((naked)) save_register_on_stack() {
    __asm volatile(
        "stp x0, x1, [sp, #-16]!\n"
        "stp x2, x3, [sp, #-16]!\n"
        "stp x4, x5, [sp, #-16]!\n"
        "stp x6, x7, [sp, #-16]!\n"
        "stp x8, x9, [sp, #-16]!\n"
        "stp x10, x11, [sp, #-16]!\n"
        "stp x12, x13, [sp, #-16]!\n"
        "stp x14, x15, [sp, #-16]!\n"
        "stp x16, x17, [sp, #-16]!\n"
        "stp x18, x19, [sp, #-16]!\n"
        "stp x20, x21, [sp, #-16]!\n"
        "stp x22, x23, [sp, #-16]!\n"
        "stp x24, x25, [sp, #-16]!\n"
        "stp x26, x27, [sp, #-16]!\n"
        "stp x28, x29, [sp, #-16]!\n"
        "str x30, [sp, #-8]!\n"       // LR отдельно

        "mov x0, sp\n"
        "ret"
        ::: "memory"
    );
}


void __attribute__((naked)) set_sp_and_load_register_from_stack(void* sp) {
    __asm volatile(
        "mov sp, x0\n"

        "ldr x30, [sp], #8\n"        // LR первым
        "ldp x28, x29, [sp], #16\n"
        "ldp x26, x27, [sp], #16\n"
        "ldp x24, x25, [sp], #16\n"
        "ldp x22, x23, [sp], #16\n"
        "ldp x20, x21, [sp], #16\n"
        "ldp x18, x19, [sp], #16\n"
        "ldp x16, x17, [sp], #16\n"
        "ldp x14, x15, [sp], #16\n"
        "ldp x12, x13, [sp], #16\n"
        "ldp x10, x11, [sp], #16\n"
        "ldp x8, x9, [sp], #16\n"
        "ldp x6, x7, [sp], #16\n"
        "ldp x4, x5, [sp], #16\n"
        "ldp x2, x3, [sp], #16\n"
        "ldp x0, x1, [sp], #16\n"
        "ret\n"
        ::: "memory"
    );
}

void switch_to_next() {
    int next_ctx = current_ctx + 1;
    if (next_ctx > CONTEXT_ARR_CAPACITY - 1) {
        next_ctx = 1;  
    }
    printf("next_ctx : %d", next_ctx);

    for (int tries = 0; tries < CONTEXT_ARR_CAPACITY; tries++) {
        if (!g_ctxs[next_ctx].is_done && g_ctxs[next_ctx].sp != NULL) {
            current_ctx = next_ctx;  
            set_sp_and_load_register_from_stack(g_ctxs[next_ctx].sp);
            return;         
        }
        next_ctx = (next_ctx + 1) % CONTEXT_ARR_CAPACITY;
        if (next_ctx == 0) next_ctx = 1; 
    }
    
    current_ctx = 0;
    set_sp_and_load_register_from_stack(g_ctxs[0].sp);
}

void yield() {
    printf("current_ctx : %zu", current_ctx);
    g_ctxs[current_ctx].sp = save_register_on_stack();
    switch_to_next();
}

void coroutine_done() {
    if (g_ctxs[current_ctx].is_main || g_ctxs[current_ctx].is_done) {
        return; // main context must exist forever or context is done alredy
    }

    g_ctxs[current_ctx].is_done = true;
    free(g_ctxs[current_ctx].stack_base);
    switch_to_next();
}

//-------------------------------------------------------------------------

void include_main_to_context() {
    stack_info_t info; 
    get_stack_info(&info);
    
    void* top_of_main = get_top_of_main_stack();
    size_t main_stack_size = get_main_stack_size();
    void* base_of_main = (char*)top_of_main - main_stack_size;
    
    g_ctxs[0].sp = (void*)info.sp;
    g_ctxs[0].top_of_stack = top_of_main;
    g_ctxs[0].stack_base = base_of_main;  
    g_ctxs[0].is_main = true;
    g_ctxs[0].is_done = false;
}


void add_corutine(void (*func)()) {
    if (current_ctx + 1 > CONTEXT_ARR_CAPACITY - 1) {
        return;
    }

    current_ctx++;
    void *stack = malloc(MIN_STACK_SIZE);
    g_ctxs[current_ctx].stack_base = stack;
    g_ctxs[current_ctx].top_of_stack = (char*)stack + (MIN_STACK_SIZE);
    uint64_t* stack_top = (uint64_t*)g_ctxs[current_ctx].top_of_stack;
    stack_top[-1] = (uint64_t)coroutine_done;
    stack_top[-2] = (uint64_t)func;
    g_ctxs[current_ctx].sp = &stack_top[-2];
}

void test_coroutine1() {
   for (int i = 0; i < 3; i++) {
       printf("Корутина 1, итерация %d\n", i);
       yield();
   }
   printf("Корутина 1 завершена\n");
}

void test_coroutine2() {
   for (int i = 0; i < 2; i++) {
       printf("Корутина 2, итерация %d\n", i);
       yield();
   }
   printf("Корутина 2 завершена\n");
}

int main() {
    printf("test");
    include_main_to_context();
    add_corutine(test_coroutine1);
    add_corutine(test_coroutine2);
    current_ctx = 0;
    for (int i = 0; i < 10; i++) {
       yield();
    }
   
    printf("Main завершен\n");
    return 0;
}

