/* memtest.c : memory manager tests */

#include <kerneltest.h>
#include <xeroskernel.h>
#include <i386.h>

extern long freemem;
extern char *maxaddr;

static void mem_test_1(void);
static void mem_test_2(void);

void run_mem_tests(void) {
    mem_test_1();
    mem_test_2();
    for(;;);
}

void mem_test_1(void) {
    mem_dump();
    void *address1 = kmalloc(2000);
    mem_dump();
    void *address2 = kmalloc(2000);
    mem_dump();
    kfree(address1);
    mem_dump();
    kfree(address2);
    mem_dump();
    kprintf("MEM_TEST_1 FINISHED\n");
}

void mem_test_2(void) {
    void *address1 = kmalloc(0);
    ASSERT_EQUAL(address1, 0);
    int size1 = 16 * 8;
    int size2 = 16 * 8;
    void *addresses[size1 + size2];
    for(int i=0; i<size1; i++) {
        addresses[i] = kmalloc(4800);
    }

    for(int i=0; i<size2; i++) {
        addresses[size1 + i] = kmalloc(19000);
    }

    address1 = kmalloc(100000);
    ASSERT_EQUAL(address1, 0);
    for(int i=0; i<size1+size2; i++) {
        kfree(addresses[i]);
    }

    for(int i=0; i<size1; i++) {
        addresses[i] = kmalloc(4800);
    }

    for(int i=0; i<size2; i++) {
        addresses[size1 + i] = kmalloc(19000);
    }

    for(int i=0; i<size1+size2; i++) {
        kfree(addresses[size1 + size2 - i - 1]);
    }

    mem_dump();
    kprintf("MEM_TEST_2 FINISHED\n");
}
