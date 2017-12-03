/* proctest.c : process management tests */

#include <kerneltest.h>
#include <xeroskernel.h>
#include <i386.h>

static void proc_test_1(void);

static void counter(void);
static void increment(void);
static void decrement(void);

static int count;

void run_proc_tests(void) {
    proc_test_1();
}

void proc_test_1(void) {
    create(counter, DEFAULT_STACK_SIZE);
    kprintf("PROC TEST 1 FINISHED\n");
}

void counter(void) {
    count = 0;
    syscreate(increment, DEFAULT_STACK_SIZE);
    syscreate(increment, DEFAULT_STACK_SIZE);
    syscreate(decrement, DEFAULT_STACK_SIZE);
    syscreate(decrement, DEFAULT_STACK_SIZE);
    for(;;) sysyield();
}

void increment(void) {
    for(int i=0; i<50; i++) {
        count++;
        kprintf("count: %d\n", count);
        sysyield();
    }
    sysstop();
}

void decrement(void) {
    for(int i=0; i<50; i++) {
        count--;
        kprintf("count: %d\n", count);
        sysyield();
    }
    sysstop();
}
