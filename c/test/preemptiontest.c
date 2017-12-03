/* preemptiontest.c : Preemption tests
 */

#include <xeroskernel.h>
#include <pcb.h>

static void preemption_test_1(void);

static void producer(void);
static void consumer(void);

/**
 * Root process launched on system boot. Spawns additional processes
 */
void run_preemption_tests(void) {
    create(preemption_test_1, DEFAULT_STACK_SIZE);
}

void preemption_test_1(void) {
    syscreate(producer, DEFAULT_STACK_SIZE);
    syscreate(consumer, DEFAULT_STACK_SIZE);
    kprintf("PREEMPTION TEST 1 FINISHED\n");
}

/* Process created by root */
void producer(void) {
    for(;;) {
        sysputs("Happy 101st ");
        syssleep(1000);
    }
}
/* Process created by root */
void consumer(void) {
    for(;;) {
        sysputs("Birthday UBC,  ");
        syssleep(1000);
    }
}

