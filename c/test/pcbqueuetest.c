/* pcbqueuetest.c : pcb queue tests */
#include <kerneltest.h>
#include <xeroskernel.h>
#include <i386.h>

#include <pcb.h>

static void queue_test_1(void);
static void queue_test_2(void);
static void queue_test_3(void);
static void queue_test_4(void);

void run_queue_tests(void) {
    queue_test_1();
    queue_test_2();
    queue_test_3();
    queue_test_4();
    for(;;);
}

void queue_test_1(void) {
    pcb_queue_t *queue = init_pcb_queue();
    int size;
    size = pcb_size(queue);
    ASSERT_EQUAL(size, 0);
    pcb_t *entry = (pcb_t *) kmalloc(sizeof(pcb_t));
    pcb_offer(queue, entry);
    size = pcb_size(queue);
    ASSERT_EQUAL(size, 1);
    pcb_t *result = pcb_poll(queue);
    size = pcb_size(queue);
    ASSERT_EQUAL(size, 0);
    ASSERT_EQUAL(result, entry);
    kfree(entry);
    kfree(queue);
    kprintf("QUEUE_TEST_1 FINISHED\n");
}

void queue_test_2(void) {
    pcb_queue_t *queue = init_pcb_queue();
    pcb_t *pcb_array[PCB_TABLE_SIZE];
    for(int i=0; i<PCB_TABLE_SIZE; i++) {
        pcb_array[i] = (pcb_t *) kmalloc(sizeof(pcb_t));
        pcb_array[i]->pid = i+1;
        pcb_offer(queue, pcb_array[i]);
    }

    int size = pcb_size(queue);
    ASSERT_EQUAL(size, PCB_TABLE_SIZE);
    for(int i=0; i<PCB_TABLE_SIZE; i++) {
        ASSERT_EQUAL(pcb_array[i], pcb_poll(queue));
        kfree(pcb_array[i]);
    }

    size = pcb_size(queue);
    ASSERT_EQUAL(size, 0);
    kfree(queue);
    kprintf("QUEUE_TEST_2 FINISHED\n");
}

void queue_test_3(void) {
    pcb_queue_t *queue = init_pcb_queue();
    pcb_t *pcb_array[3];
    for(int i=0; i<3; i++) {
        pcb_array[i] = (pcb_t *) kmalloc(sizeof(pcb_t));
        pcb_array[i]->pid = i+1;
        pcb_offer(queue, pcb_array[i]);
    }

    int size = pcb_size(queue);
    ASSERT_EQUAL(size, 3);
    dump_pcb_queue(queue);
    for(int i=0; i<PCB_TABLE_SIZE; i++) {
        pcb_t *pcb = pcb_poll(queue);
        pcb_offer(queue, pcb);
    }

    for(int i=0; i<3; i++) {
        kfree(pcb_array[i]);
    }

    dump_pcb_queue(queue);
    kfree(queue);
    kprintf("QUEUE_TEST_3 FINISHED\n");
}

void queue_test_4(void) {
    pcb_queue_t *queue = init_pcb_queue();
    pcb_t *pcb_array[PCB_TABLE_SIZE];
    for(int i=0; i<PCB_TABLE_SIZE; i++) {
        pcb_array[i] = (pcb_t *) kmalloc(sizeof(pcb_t));
        pcb_array[i]->pid = i+1;
        pcb_offer(queue, pcb_array[i]);
    }

    int size = pcb_size(queue);
    ASSERT_EQUAL(size, PCB_TABLE_SIZE);
    for(int i=PCB_TABLE_SIZE/2+1; i<PCB_TABLE_SIZE; i++) {
        pcb_remove(queue, pcb_array[i]);
    }

    for(int i=PCB_TABLE_SIZE/2; i>=0; i--) {
        pcb_remove(queue, pcb_array[i]);
    }

    size = pcb_size(queue);
    ASSERT_EQUAL(size, 0);
    kfree(queue);
    kprintf("QUEUE_TEST_4 FINISHED\n");
}
