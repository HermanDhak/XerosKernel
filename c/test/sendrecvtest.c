/* sendrevctest.c : send and revc tests
 */

#include <kerneltest.h>
#include <xeroskernel.h>
#include <xeroslib.h>
#include <i386.h>

#include <pcb.h>

static void sendrecv_test_1(void);
static void sendrecv_test_2(void);
static void sendrecv_test_3(void);

static void sendrecv1(void);
static void sender1(void);
static void receiver1(void);

static void sendrecv2(void);
static void sender2(void);
static void sender3(void);
static void receiver2(void);

static void sendrecv3(void);
static void sender4(void);
static void receiver3(void);

static pid_t rootpid1;
static pid_t sendpid1;
static pid_t recvpid1;

static pid_t rootpid2;
static pid_t sendpid2;
static pid_t sendpid3;
static pid_t recvpid2;

static pid_t rootpid3;
static pid_t sendpid4;
static pid_t recvpid3;

void run_sendrecv_tests(void) {
    sendrecv_test_1();
    sendrecv_test_2();
    sendrecv_test_3();
}

void sendrecv_test_1(void) {
    create(sendrecv1, DEFAULT_STACK_SIZE);
}

void sendrecv_test_2(void) {
    create(sendrecv2, DEFAULT_STACK_SIZE);
}

void sendrecv_test_3(void) {
    create(sendrecv3, DEFAULT_STACK_SIZE);
}

void sendrecv1(void) {
    rootpid1 = sysgetpid();
    syscreate(sender1, DEFAULT_STACK_SIZE);
    syscreate(receiver1, DEFAULT_STACK_SIZE);
    kprintf("SENDRECV TEST 1 FINISHED\n");
}

void sendrecv2(void) {
    rootpid2 = sysgetpid();
    syscreate(receiver2, DEFAULT_STACK_SIZE);
    syscreate(sender2, DEFAULT_STACK_SIZE);
    syscreate(sender3, DEFAULT_STACK_SIZE);
    kprintf("SENDRECV TEST 2 FINISHED\n");
}

void sendrecv3(void) {
    rootpid3 = sysgetpid();
    syscreate(receiver3, DEFAULT_STACK_SIZE);
    syscreate(sender4, DEFAULT_STACK_SIZE);
    kprintf("SENDRECV TEST 3 FINISHED\n");
}

void sender1(void) {
    sendpid1 = sysgetpid();
    sysyield();
    void *buffer = "hello world 1!\n";
    syssend(recvpid1, buffer, 16);
}

void receiver1(void) {
    recvpid1 = sysgetpid();
    sysyield();
    void *buffer = (void *) kmalloc(16);
    sysrecv(&sendpid1, buffer, 16);
    sysputs(buffer);
}

void sender2(void) {
    sendpid2 = sysgetpid();
    sysyield();
    void *buffer = "hello world 2!\n";
    syssend(sendpid3, buffer, 16);
}

void sender3(void) {
    sendpid3 = sysgetpid();
    sysyield();
    void *buffer = "hello world 2!\n";
    syssend(sendpid2, buffer, 16);
}

void receiver2(void) {
    recvpid2 = sysgetpid();
    sysyield();
    void *buffer = (void *)kmalloc(16);
    pid_t i = 0;
    sysrecv(&i, buffer, 16);
    sysputs(buffer);
}

void sender4(void) {
    sendpid4 = sysgetpid();
    void *buffer = "hello world 3!\n";
    int status = syssend(sendpid4, buffer, 16);
    char message[32];
    sprintf(message, "Error code: %d\n", status);
    sysputs(message);
}

void receiver3(void) {
    recvpid3 = sysgetpid();
    void *buffer = (void *)kmalloc(16);
    int status = sysrecv(&recvpid3, buffer, 16);
    char message[32];
    sprintf(message, "Error code: %d\n", status);
    sysputs(message);
}
