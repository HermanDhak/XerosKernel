/* devicetest.c : Signal handling tests
 */

#include <xeroskernel.h>
#include <kerneltest.h>
#include <xeroslib.h>
#include <pcb.h>

static void root_test(void);
static void test_device_open(void);
static void test_device_write(void);
static void test_device_ioctl(void);
static void test_device_read(void);
static void test_cpu_times(void);

static void busy_loop(void);

void run_device_tests(void) {
    create(root_test, DEFAULT_STACK_SIZE);
}

void root_test(void) {
    test_device_open();
    test_device_write();
    test_device_ioctl();
    test_cpu_times();
    test_device_read();

    sysputs("Done all device tests. Looping.\n");
    for(;;);
}

void test_device_open(void) {
    ASSERT_EQUAL(sysopen(-1), -1);
    ASSERT_EQUAL(sysopen(NUM_DEVICES), -1);
    ASSERT_EQUAL(sysopen(999), -1);
}

void test_device_write(void) {
    void *buffer[256];
    ASSERT_EQUAL(syswrite(-1, buffer, 256), -1);
    ASSERT_EQUAL(syswrite(0, buffer, 256), -1);
    ASSERT_EQUAL(syswrite(999, buffer, 256), -1);
    int fd = sysopen(DEV_ID_KEYBOARD);
    ASSERT_EQUAL(syswrite(fd-1, buffer, 256), -1);
    ASSERT_EQUAL(syswrite(fd+1, buffer, 256), -1);
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysclose(fd), -1);
}

void test_device_ioctl(void) {
    int fd = sysopen(DEV_ID_KEYBOARD);
    ASSERT_EQUAL(sysioctl(fd, 0), -1);
    ASSERT_EQUAL(sysioctl(fd, 52), -1);
    ASSERT_EQUAL(sysioctl(fd, 57), -1);
    ASSERT_EQUAL(sysclose(fd), 0);
}

void test_device_read(void) {
    int fd = sysopen(DEV_ID_KEYBOARD);
    syssleep(3000);
    char buffer[2];
    ASSERT_EQUAL(sysread(fd, buffer, 2), 2);
    ASSERT_EQUAL(sysread(fd, buffer, 2), 2);
    ASSERT_EQUAL(sysclose(fd), 0);
}

void test_cpu_times(void) {
    syscreate(busy_loop, DEFAULT_STACK_SIZE);
    sysyield();
    syssleep(5000);
    for(int i=0; i<1000; i++) {
        sysputs("                                    \n");
    }
    processStatuses psTab;
    int procs = sysgetcputimes(&psTab);
    ASSERT_EQUAL(procs, 1);
    char buff[100];
    for(int j = 0; j <= procs; j++) {
        sprintf(buff, "%4d    %4d    %10d\n", psTab.pid[j], psTab.status[j], psTab.cpuTime[j]);
        kprintf(buff);
    }
}

void busy_loop(void) {
    int count = 0;
    for(;;) {
        count++;
    }
}
