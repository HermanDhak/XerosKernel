/* signaltest.c : Signal handling tests
 */

#include <xeroskernel.h>
#include <kerneltest.h>
#include <xeroslib.h>
#include <pcb.h>

static void signaltest_syskill(void);
static void signaltest_syshandler(void);
static void signaltest_priorities(void);
static void signaltest_wait(void);
static void signaltest_blocked(void);

static void root_test(void);
static void root_function(void);
static void busy_loop(void);
static void sender(void);
static void receiver(void);
static void low_pri(void* cntx);
static void medium_pri(void* cntx);
static void high_pri(void* cntx);

static int signal_handled = 0;

void run_signal_tests(void) {
    create(root_test, DEFAULT_STACK_SIZE);
}

void root_test(void) {
    signaltest_priorities();
    signaltest_syskill();
    signaltest_syshandler();
    signaltest_wait();
    signaltest_blocked();

    sysputs("Done all signal tests. Looping.\n");
    for(;;);
}

void signaltest_priorities(void) {
    int pid = syscreate(root_function, DEFAULT_STACK_SIZE);
    sysyield();
    syskill(pid, 15);
    sysputs("send kill signal 15\n");
    sysyield();
    syskill(pid, 0);
    sysputs("send kill signal 0\n");
    sysyield();
    syskill(pid, 30);
    sysputs("send kill signal 30\n");
    sysyield();
    for(int i=0; i<11; i++) {
        sysyield();
    }
}

void signaltest_syskill(void) {
    sysputs("creating proc, it sets up its handlers\n");
    int pid = sysgetpid();
    ASSERT(pid > 0);

    kprintf("testing syskill error cases\n");
    ASSERT_EQUAL(syskill(-1, 0), SYSKILL_TARGET_DNE);
    ASSERT_EQUAL(syskill(9999, 0), SYSKILL_TARGET_DNE);
    ASSERT_EQUAL(syskill(pid, -1), SYSKILL_SIG_INVALID);
    ASSERT_EQUAL(syskill(pid, 32), SYSKILL_SIG_INVALID);

    sysputs("testing ignore signal returns success\n");
    int result = syskill(pid, 11);
    ASSERT_EQUAL(result, 0);
}

/**
 * Tests error responses of syshandler, and all other basics
 */
void signaltest_syshandler(void) {
    funcptr_args oldHandler;

    // error codes
    sysputs("Testing invalid signals\n");
    ASSERT_EQUAL(syssighandler(-1, &low_pri, &oldHandler),
                 INVALID_SIGNAL);
    ASSERT_EQUAL(syssighandler(32, &low_pri, &oldHandler),
                 INVALID_SIGNAL);

    sysputs("Testing invalid functions\n");
    ASSERT_EQUAL(syssighandler(0, &low_pri, NULL),
                 SYSHANDLER_OLDHANDLER_INVALID);
    ASSERT_EQUAL(syssighandler(0, (funcptr_args)-1, &oldHandler),
                 SYSHANDLER_NEWHANDLER_INVALID);

    // change signal handlers, get back old ones
    sysputs("Testing setting up signal handlers\n");
    ASSERT_EQUAL(syssighandler(0, &low_pri, &oldHandler), 0);
    ASSERT_EQUAL(oldHandler, NULL);
    ASSERT_EQUAL(syssighandler(0, &high_pri, &oldHandler), 0);
    ASSERT_EQUAL(oldHandler, low_pri);
    ASSERT_EQUAL(syssighandler(30, &high_pri, &oldHandler), 0);
    ASSERT_EQUAL(oldHandler, NULL);
    ASSERT_EQUAL(syssighandler(0, &low_pri, &oldHandler), 0);
    ASSERT_EQUAL(oldHandler, high_pri);
    ASSERT_EQUAL(syssighandler(15, &medium_pri, &oldHandler), 0);
    ASSERT_EQUAL(oldHandler, NULL);
}

void signaltest_wait(void) {
    int pid = syscreate(busy_loop, DEFAULT_STACK_SIZE);
    char message[256];
    sprintf(message, "return value of syswait: %d\n", syswait(pid));
    sysputs(message);
    pid = 9999;
    sprintf(message, "return value of syswait: %d\n", syswait(pid));
    sysputs(message);
}

void signaltest_blocked(void) {
    int send_pid = syscreate(sender, DEFAULT_STACK_SIZE);
    int recv_pid = syscreate(receiver, DEFAULT_STACK_SIZE);
    syskill(send_pid, 0);
    syskill(recv_pid, 0);
}

void root_function(void) {
    funcptr_args oldHandler;
    syssighandler(0, &low_pri, &oldHandler);
    syssighandler(15, &medium_pri, &oldHandler);
    syssighandler(30, &high_pri, &oldHandler);
    for(;;) {
        sysyield();
    }
}

void busy_loop(void) {
    for(int i=0; i<12; i++) {
        sysyield();
    }
}

void sender(void) {
    funcptr_args oldHandler;
    syssighandler(0, &low_pri, &oldHandler);
    syssighandler(15, &medium_pri, &oldHandler);
    syssighandler(30, &high_pri, &oldHandler);
    void *buffer[256];
    char message[256];
    sprintf(message, "return value of send: %d\n", syssend(1, buffer, 19));
    sysputs(message);
}

void receiver(void) {
    funcptr_args oldHandler;
    syssighandler(0, &low_pri, &oldHandler);
    syssighandler(15, &medium_pri, &oldHandler);
    syssighandler(30, &high_pri, &oldHandler);
    void *buffer[256];
    char message[256];
    pid_t pid = 1;
    sprintf(message, "return value of recv: %d\n", sysrecv(&pid, buffer, 19));
    sysputs(message);
}

void low_pri(void* cntx) {
    for(int i=0; i<4; i++) {
        sysputs("Signal 0 running\n");
        sysyield();
    }
    signal_handled = 0xCAFECAFE;
}

void medium_pri(void* cntx) {
    for(int i=0; i<4; i++) {
        sysputs("Signal 15 running\n");
        sysyield();
    }
    signal_handled  = 0xBEEFBEEF;
}

void high_pri(void* cntx) {
    for(int i=0; i<4; i++) {
        sysputs("Signal 30 running\n");
        sysyield();
    }
    signal_handled = 0xDEADBEEF;
}

