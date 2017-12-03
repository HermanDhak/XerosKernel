/* pcb.c : process control block and process queue functions
 * 
 * Called from outside:
 *  initpcb() - Intialize the PCB table and all process queues
 *  add_pcb_to_stopped_queue() - Adds the pcb to the stopped queue
 *  add_pcb_to_ready_queue() - Adds the pcb to the ready queue
 *  remove_pcb_from_ready_queue() - Removes the pcb from the ready queue
 *  add_pcb_to_blocked_queue() - Adds the pcb to the blocked queue
 *  remove_pcb_from_blocked_queue() - Removes the pcb from the blocked queue
 *  peek_next_sender() - Peeks the next sender pcb
 *  peek_any_receiver() - Peeks the next any receiver pcb
 *  unblock_pcb_waiting_for_pid() - Unblock the pcbs waiting for the given pid from the blocked queue and adds it to the ready queue
 *  get_next_pcb() - Get the next PCB from the process queue
 *  get_free_pcb() - Return an available PCB to use for a new process from PCB table
 *  pid_to_pcb() -  Returns the pcb associated with the given pid if it's valid 
 *  cleanup_pcb() - Free memory allocated to this pcb
 *  dump_stopped_queue() - Print stopped queue to console
 *  dump_ready_queue() - Print process queue to console
 *  dump_blocked_queue() - Print blocked queue to console
 *  fill_processStatus() - Fills processStatus struct with various pcb info
 */

#include <xeroskernel.h>
#include <pcb.h>

static pcb_queue_t *stopped_queue;
static pcb_queue_t *ready_queue;
static pcb_queue_t *blocked_queue;
static pcb_t pcb_array[PCB_TABLE_SIZE];

/**
 * Initalizes the pcb array and the process queues used by the dispatcher
 */
void initpcb(void) {
    stopped_queue = (pcb_queue_t *) init_pcb_queue();
    ready_queue = (pcb_queue_t *) init_pcb_queue();
    blocked_queue = (pcb_queue_t *) init_pcb_queue();
    for(int i = 0; i < PCB_TABLE_SIZE; i++) {
        pcb_array[i].pid = i+1;
        pcb_array[i].blocked_id = 0;
        pcb_array[i].cpu_time = 0;
        pcb_array[i].blocked_status = BLOCKED_STATUS_NONE;
        pcb_array[i].state = PROC_STATE_STOPPED;
        pcb_offer(stopped_queue, &pcb_array[i]);
    }
    
    sleepinit();
}

/**
 * Adds the pcb to the stopped queue
 */
void add_pcb_to_stopped_queue(pcb_t *pcb) {
    pcb->pid = (pcb->pid + PCB_TABLE_SIZE - 1) % PID_MAX + 1;
    pcb->cpu_time = 0;
    pcb->state = PROC_STATE_STOPPED;
    pcb_offer(stopped_queue, pcb);
}

/**
 * Adds the pcb to the ready queue
 */
void add_pcb_to_ready_queue(pcb_t *pcb) {
    ASSERT(pcb != NULL);
    pcb->state = PROC_STATE_READY;
    pcb->blocked_status = BLOCKED_STATUS_NONE;
    pcb_offer(ready_queue, pcb);
}

/**
 * Removes the pcb from the ready queue
 */
bool remove_pcb_from_ready_queue(pcb_t *pcb) {
    ASSERT(pcb != NULL);
    return pcb_remove(ready_queue, pcb);
}

/**
 * Adds the pcb to the blocked queue
 */
void add_pcb_to_blocked_queue(pcb_t *pcb) {
    ASSERT(pcb != NULL);
    pcb->state = PROC_STATE_BLOCKED;
    pcb_offer(blocked_queue, pcb);
}

/**
 * Removes the pcb from the blocked queue
 */
bool remove_pcb_from_blocked_queue(pcb_t *pcb) {
    ASSERT(pcb != NULL);
    return pcb_remove(blocked_queue, pcb);
}

/**
 * Peeks the next sender pcb
 */
pcb_t *peek_next_sender(void) {
    int size = pcb_size(blocked_queue);
    pcb_t *result = NULL;
    for(int i=0; i<size; i++) {
        pcb_t *entry = pcb_poll(blocked_queue);
        if(result == NULL && entry->blocked_status == BLOCKED_STATUS_SEND) {
            result = entry;
        }
        pcb_offer(blocked_queue, entry);
    }
    return result;
}

/**
 * Peeks the next any receiver pcb
 */
pcb_t *peek_any_receiver(void) {
    int size = pcb_size(blocked_queue);
    pcb_t *result = NULL;
    for(int i=0; i<size; i++) {
        pcb_t *entry = pcb_poll(blocked_queue);
        if(result == NULL && entry->blocked_id == 0) {
            result = entry;
        }
        pcb_offer(blocked_queue, entry);
    }
    return result;
}

/**
 * Unblock the pcbs waiting for the given pid from
 * the blocked queue and adds it to the ready queue
 */
void unblock_pcb_waiting_for_pid(pid_t pid) {
    int size = pcb_size(blocked_queue);
    for(int i=0; i<size; i++) {
        pcb_t *entry = pcb_poll(blocked_queue);
        if(entry->blocked_id == pid) {
            entry->blocked_id = 0;
            add_pcb_to_ready_queue(entry);
        } else {
            add_pcb_to_blocked_queue(entry);
        }
    }
}

/**
 * Get the next pcb in the ready queue
 */
pcb_t *get_next_pcb(void) {
    pcb_t *next_pcb = pcb_poll(ready_queue);
    next_pcb->state = PROC_STATE_RUNNING;
    next_pcb->cpu_time++;
    return next_pcb;
}

/**
 * Returns the next free available pcb from the pcb table.
 * Returns null if none available.
 */
pcb_t *get_free_pcb(void) {
    return pcb_poll(stopped_queue);
}

/**
 * Returns the process if the pid exists and the process is not stopped,
 * otherwise return NULL.
 */
pcb_t *pid_to_pcb(pid_t pid) {
    if (pid >= 1) {
        pcb_t *pcb = &pcb_array[(pid - 1) % PCB_TABLE_SIZE];
        if (pcb->state != PROC_STATE_STOPPED) {
            return pcb;
        }
    }

    return NULL;
}

/**
 * Frees the allocated memory associated with this pcb
 */
void cleanup_pcb(pcb_t *pcb) {
    ASSERT(pcb != NULL);
    
    remove_pcb_from_ready_queue(pcb);
    remove_pcb_from_blocked_queue(pcb);
    remove_pcb_from_sleep_queue(pcb);
    
    unblock_pcb_waiting_for_pid(pcb->pid);
    add_pcb_to_stopped_queue(pcb);
    
    /* Free all alloced mem */
    kfree(pcb->stack_start);
    kfree(pcb->signal_table);
}

/**
 * Print stopped queue to console
 */
void dump_stopped_queue(void) {
	kprintf("Stopped queue: \n");
    dump_pcb_queue(stopped_queue);
}

/**
 * Print ready queue to console
 */
void dump_ready_queue(void) {
	kprintf("Ready queue: \n");
	dump_pcb_queue(ready_queue);
}

/**
 * Print blocked queue to console
 */
void dump_blocked_queue(void) {
	kprintf("Blocked queue: \n");
	dump_pcb_queue(blocked_queue);
}

/**
 * fill process status fills the struct processStatuses with
 * information about the process and their status
 */
int fill_processStatus(pcb_t *pcb, processStatuses *ps) {
  int i;
  int currentSlot = 0;
  pcb_t *idle_proc = get_idleproc();
  ps->pid[currentSlot] = idle_proc->pid;
  ps->status[currentSlot] = idle_proc->state;
  ps->cpuTime[currentSlot] = idle_proc->cpu_time * MS_PER_CLOCK_TICK;
  for (i = 0; i < PCB_TABLE_SIZE; i++) {
    if (pcb_array[i].state != PROC_STATE_STOPPED) {
      // fill in the table entry
      currentSlot++;
      ps->pid[currentSlot] = pcb_array[i].pid;
      ps->status[currentSlot] = pcb->pid == pcb_array[i].pid ? PROC_STATE_RUNNING: pcb_array[i].state + pcb_array[i].blocked_status;
      ps->cpuTime[currentSlot] = pcb_array[i].cpu_time * MS_PER_CLOCK_TICK;
    }
  }

  return currentSlot;
}

