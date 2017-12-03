/* pcbqueue.c : process queue related functions 
 *
 * Called from pcb.c:
 *  init_pcb_queue() - Initialize a queue used to hold pcb's
 *  pcb_size() - Return size of the queue
 *  pcb_poll() - Remove a pcb from the front of the queue
 *  pcb_peek() - Peek the front of the queue
 *  pcb_offer() - Add a pcb to the end of the queue
 *  pcb_remove() - Remove a pcb from the queue
 *  dump_pcb_queue() - Print the queue out to the screen
 */

#include <pcb.h>

/*
 * Allocate memory to be used by a new queue
 */
pcb_queue_t *init_pcb_queue(void) {
    pcb_queue_t *queue = (pcb_queue_t *) kmalloc(sizeof(pcb_queue_t));
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    return queue;
}

/*
 * Return the size of the given queue
 */
int pcb_size(pcb_queue_t *queue) {
    return queue->size;
}

/*
 * Remove the first pcb from the given queue
 * Return null if queue empty
 */
pcb_t *pcb_poll(pcb_queue_t *queue) {
    if(queue->head == NULL) {
        return NULL;
    }

    queue->size--;
    pcb_t *node = queue->head;
    queue->head = queue->head->next;
    if(queue->head == NULL) {
        queue->tail = NULL;
    }

    node->next = NULL;
    return node;
}

/*
 * Peek the front of the queue
 */
pcb_t *pcb_peek(pcb_queue_t *queue) {
    return queue->head;
}

/*
 * Add the given pcb to the given queue
 */
void pcb_offer(pcb_queue_t *queue, pcb_t *entry) {
    queue->size++;
    if(queue->head == NULL) {
        queue->head = entry;
        queue->tail = entry;
        entry->next = NULL;
        return;
    }
    
    queue->tail->next = entry;
    queue->tail = queue->tail->next;
    entry->next = NULL;
    return;
}

/*
 * Removes a pcb from within queue
 */
bool pcb_remove(pcb_queue_t *queue, pcb_t *entry) {
    pcb_t *prev = NULL;
    pcb_t *curr = queue->head;
    while(curr != NULL && curr != entry) {
        prev = curr;
        curr = curr->next;
    }

    if(curr == NULL) {
        return FALSE;
    }

    if(prev == NULL) {
        pcb_poll(queue);
        return TRUE;
    }

    if(curr->next == NULL) {
        queue->tail = prev;
    }

    prev->next = curr->next;
    curr->next = NULL;
    queue->size--;
    return TRUE;
}

/*
 * Print the pid's of pcb's in the queue to the screen
 */
void dump_pcb_queue(pcb_queue_t *queue) {
    pcb_t *curr = queue->head;
    while(curr != NULL) {
        LOG("[pid: %d, ret: %d\n], ", curr->pid, curr->ret);
        curr = curr->next;
    }

    kprintf("\n");
    return;
}
