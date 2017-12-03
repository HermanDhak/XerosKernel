/* sleep.c : sleep device 
 *
 * Called from outside:
 *  sleepinit() - Intialize the sleep queue
 *  sleep() - The kernel implementation of the sleep syscall
 *  tick() - Actions that need to be performed on clock tick ie. wake sleeping processes
 *  wake() - Wake sleeping processes and adds it to the ready queue
 *  dump_sleep_queue() - Dumps the sleep queue to stdout
 */

#include <xeroskernel.h>
#include <xeroslib.h>
#include <pcb.h>

pcb_queue_t *sleep_queue;

/*
 * Intialize the sleep queue
 */
void sleepinit(void) {
    sleep_queue = (pcb_queue_t *) init_pcb_queue();
}

/*
 * The kernel implementation of the sleep syscall
 */
void sleep(pcb_t *pcb, unsigned int milliseconds) {
    pcb->ret = milliseconds / MS_PER_CLOCK_TICK + (milliseconds % MS_PER_CLOCK_TICK ? 1 : 0);
    add_pcb_to_sleep_queue(pcb);
}

/*
 * Actions that need to be performed on clock tick ie. wake sleeping processes
 */
void tick(void) {
    pcb_t *head = peek_pcb_from_sleep_queue();
    if(head == NULL) {
        return;
    }

    head->ret--;
    while(head != NULL && head->ret <= 0) {
        wake(head);
        head = peek_pcb_from_sleep_queue();
    }
}

/*
 * Wake sleeping processes and adds it to the ready queue
 */
void wake(pcb_t *pcb) {
    remove_pcb_from_sleep_queue(pcb);
    add_pcb_to_ready_queue(pcb);
}

/*
 * Add the given pcb to the delta queue
 */
void add_pcb_to_sleep_queue(pcb_t *entry) {
    ASSERT(entry->next == NULL);
    entry->state = PROC_STATE_BLOCKED;
    entry->blocked_status = BLOCKED_STATUS_SLEEP;
    pcb_t *prev = NULL;
    pcb_t *curr = sleep_queue->head;
    while(curr != NULL && curr->ret < entry->ret) {
        entry->ret -= curr->ret;
        prev = curr;
        curr = curr->next;
    }

    if(curr == NULL) {
        pcb_offer(sleep_queue, entry);
        return;
    }

    curr->ret -= entry->ret;
    entry->next = curr;
    sleep_queue->size++;
    if(prev == NULL) {
        sleep_queue->head = entry;
        return;
    }

    prev->next = entry;
    return;
}

/*
 * Remove the pcb from the sleep queue
 */
bool remove_pcb_from_sleep_queue(pcb_t *pcb) {
    return pcb_remove(sleep_queue, pcb);
}

/*
 * Peeks the next pcb from sleep queue
 */
pcb_t *peek_pcb_from_sleep_queue(void) {
    return pcb_peek(sleep_queue);
}

/*
 * Dumps the sleep queue to stdout
 */
void dump_sleep_queue(void) {
    dump_pcb_queue(sleep_queue);
} 
