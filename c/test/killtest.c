/* killtest.c : kill tests
 */

#include <xeroskernel.h>
#include <pcb.h>

static void kill_test_1(void);

static void process1(void);
static void process2(void);
static void process3(void);
static void process4(void);

pid_t rootpid;
pid_t pid1;
pid_t pid2;
pid_t pid3;
pid_t pid4;

/**
 * Root process launched on system boot. Spawns additional processes
 */
void run_kill_tests(void) {
    create(kill_test_1, DEFAULT_STACK_SIZE);
}

void kill_test_1(void) {
    rootpid = sysgetpid();
    syscreate(process1, DEFAULT_STACK_SIZE);
    syscreate(process2, DEFAULT_STACK_SIZE);
    syscreate(process3, DEFAULT_STACK_SIZE);
    syscreate(process4, DEFAULT_STACK_SIZE);
    sysyield();

    syssleep(2000);
    syskill(pid1, KILL_SIGNAL_NUM);
    syskill(pid3, KILL_SIGNAL_NUM);
    syssleep(2000);
    kprintf("KILL TEST 1 FINISHED\n");
    syskill(rootpid, KILL_SIGNAL_NUM);
    for(;;) sysputs("forever\n");    
}

void process1(void) {
    pid1 = sysgetpid();
    sysyield();
    void *buffer = "hello world!\n";
    syssend(rootpid, buffer, 16);
}
void process2(void) {
    pid2 = sysgetpid();
    sysyield();
    void *buffer = "hello world!\n";
    syssend(pid1, buffer, 16);
}

void process3(void) {
    pid3 = sysgetpid();
    sysyield();
    void *buffer = "hello world!\n";
    syssend(rootpid, buffer, 16);
}

void process4(void) {
    pid4 = sysgetpid();
    sysyield();
    void *buffer = "hello world!\n";
    syssend(rootpid, buffer, 16);
}
