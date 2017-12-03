/* create.c : create a process
 *
 * Called from outside:
 *  create() - Create a new process and push it onto ready queue
 *  idleinit() - Creates the idle process
 *  get_idleproc() - Gets the idle process and returns it
 */

#include <xeroskernel.h>
#include <xeroslib.h>
#include <pcb.h>

#define STARTING_EFLAGS        0x00003200


static pcb_t idle_process;

/*
 * Create a new process by finding an available PCB, allocating stack memory,
 * and push it into ready queue.
 * Returns 1 if successful, 0 otherwise.
 */
int create(funcptr fp, int stack) {
    funcptr *sysstop_return_addr;

    if (fp == NULL) {
        LOG("Null function pointer passed to create!\n");
        return CREATE_FAILURE;
    }

    if (stack < DEFAULT_STACK_SIZE) {
        stack = DEFAULT_STACK_SIZE;
    }

    void *stack_start = kmalloc(stack);
    if (!stack_start) {
        return CREATE_FAILURE;
    }

    funcptr_args *signal_table = kmalloc(SIGNAL_TABLE_SIZE * sizeof(funcptr_args));
    if (!signal_table) {
        return CREATE_FAILURE;
    }
    memset(signal_table, 0, sizeof(SIGNAL_TABLE_SIZE * sizeof(funcptr_args)));

    pcb_t *new_proc = get_free_pcb();
    if (new_proc == NULL) {
        LOG("Unable to find an available PCB!\n");
        kfree(stack_start);
        kfree(signal_table);
        return CREATE_FAILURE;
    }
    

    // Storing this will make it easy to free the stack mem later
    new_proc->stack_start = stack_start;
    
    new_proc->signal_table = signal_table;
    new_proc->signals_enabled = 0;
    
    // Every process has a signal handler installed by default to terminate the process on signal 31
    new_proc->signal_table[KILL_SIGNAL_NUM] = (funcptr_args)&sysstop;

    // Set the address of sysstop() as the return address of this process
    sysstop_return_addr =
        (funcptr*)((int)stack_start + stack - sizeof(*sysstop_return_addr));
    *sysstop_return_addr = &sysstop;

    // Start stack pointer at a high address right after the context frame
    new_proc->esp = (void*)((int)sysstop_return_addr - sizeof(context_frame_t));
    context_frame_t *new_context = new_proc->esp;

    init_context_frame(new_context, fp);
    add_pcb_to_ready_queue(new_proc);

    return new_proc->pid;
}

/*
 * Creates the idle process
 */
int idleinit(void) {
    funcptr *sysstop_return_addr;

    void *stack_start = kmalloc(IDLE_PROC_STACK_SIZE);
    if (!stack_start) {
        return CREATE_FAILURE;
    }

    // Storing this will make it easy to free the stack mem later
    idle_process.stack_start = stack_start;

    // Set the address of sysstop() as the return address of this process
    sysstop_return_addr =
        (funcptr*)((int)stack_start + IDLE_PROC_STACK_SIZE - sizeof(*sysstop_return_addr));
    *sysstop_return_addr = &sysstop;

    // Start stack pointer at a high address right after the context frame
    idle_process.esp = (void*)((int)sysstop_return_addr - sizeof(context_frame_t));
    context_frame_t *new_context = idle_process.esp;

    init_context_frame(new_context, idleproc);

    idle_process.pid = 0;
    idle_process.blocked_id = 0;
    idle_process.cpu_time = 0;
    idle_process.blocked_status = BLOCKED_STATUS_NONE;
    idle_process.state = PROC_STATE_READY;
    return 0;
}

/*
 * Gets the idle process and returns it
 */
pcb_t *get_idleproc(void) {
    return &idle_process;
}

/**
 * Initialize the context frame of a new process with default values
 */
void init_context_frame(context_frame_t *context, funcptr fp) {
    memset(context, 0, sizeof(context_frame_t));
    context->iret_cs = getCS();
    context->iret_eip = (unsigned long)fp; // starting location of process code
    context->eflags = STARTING_EFLAGS;
    context->esp = (unsigned long)(context + 1);
    context->ebp = context->esp; // initially frame pointer = stack pointer
}

/**
 * The idle process
 */
void idleproc(void) {
    for(;;) {
        __asm__ volatile( " \
            hlt \n"
        );
    }
}

