#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h> // mmap here

typedef struct {
  uint64_t sp; // Stack Pointer
  uint64_t fp; // Frame Pointer (x29)
  uint64_t lr; // Link Register (x30)
} stack_info_t;

void get_stack_info(stack_info_t *info) {
  __asm volatile("mov x1, sp\n"
                 "str x1, [%0, #0]\n"   // info->sp = x1 (sp)
                 "str x29, [%0, #8]\n"  // info->fp = fp
                 "str x30, [%0, #16]\n" // info->lr = lr
                 ::"r"(info)
                 : "x1", "memory");
}

void print_stack_info(stack_info_t info) {
  printf("SP (Stack Pointer): 0x%llx\n", info.sp);
  printf("FP (Frame Pointer): 0x%llx\n", info.fp);
  printf("LR (Link Register): 0x%llx\n", info.lr);
}

void *get_top_of_main_stack() {
  pthread_t self = pthread_self();
  return pthread_get_stackaddr_np(self);
}

size_t get_main_stack_size() {
  pthread_t self = pthread_self();
  return pthread_get_stacksize_np(self);
}

//-------------------------------------------------------------------------

typedef struct {
  void *sp;
  void *ip;
  void *top_of_stack;
  void *stack_base;
} context_t;

static context_t g_ctxs[2];
static bool current_ctx;

void test_func() { printf("test of work\n"); }

void return_func() { printf("after work\n"); exit(0);}

void save_main() {
  stack_info_t info;
  get_stack_info(&info);
  size_t stack_size = get_main_stack_size();
  void *top = get_top_of_main_stack();
  g_ctxs[0].sp = (void *)info.sp;
  g_ctxs[0].top_of_stack = top;
  g_ctxs[0].stack_base = (void *)((char *)top - stack_size);
}

void *set_up_test() {
  size_t stack_size = 1024 * 4;
  void *stack = malloc(stack_size);

  // Убеждаемся, что stack_top выровнен по 16 байт
  void *stack_top = (void *)((char *)stack + stack_size);
  uintptr_t aligned_top = ((uintptr_t)stack_top) & ~0xF; // Выравниваем по 16

  g_ctxs[1].top_of_stack = (void *)aligned_top;
  g_ctxs[1].stack_base = stack;

  void **sp = (void **)aligned_top;
  sp[-1] = (void *)test_func;   // Адрес функции
  sp[-2] = (void *)return_func; // Return адрес после test_func
  sp[-3] = NULL;                   // -24 от stack_top (padding)
  sp[-4] = NULL; 
  // SP должен быть выровнен по 16 байт
  g_ctxs[1].sp = (void *)((uintptr_t)(sp - 4) & ~0xF);

  return stack;
}

int main() {
  save_main();
  void *stack = set_up_test();

  void *new_sp = g_ctxs[1].sp;

  // Дополнительная проверка выравнивания
  printf("new_sp alignment: %p (should be 16-byte aligned)\n", new_sp);
  assert(((uintptr_t)new_sp % 16) == 0);

 __asm volatile(
    "mov sp, %0\n"           // SP выровнен
    "ldr x30, [sp, #16]\n"   // return_func (SP + 16)
    "ldr x1, [sp, #24]\n"    // test_func (SP + 24)
    "add sp, sp, #32\n"      // Корректируем SP, сохраняя выравнивание
    "br x1\n"
    :
    : "r"(new_sp)
    : "x1", "x30", "memory"
);
 printf("This shouldn't print\n");
  return 0;
}
