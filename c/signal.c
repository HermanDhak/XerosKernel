/* signal.c - support for signal handling
 *  sigtramp() - sets up the signal trampoline
 *  signal() - Prepares the signal with the signalNumber to pid
 *  set_pcb_signal() - Mark signal for delivery
 *  deliver_highest_priority_signal() - Delivers any pending signal with the highest priority
 */

#include <xeroskernel.h>
#include <xeroslib.h>
#include <pcb.h>

/*
 * sets up the signal trampoline
 */
void sigtramp(funcptr_args handler, void *cntx) {
    handler(cntx);
    syssigreturn(cntx);
}

/*
 * Prepares the signal with the signalNumber to pid
 * for the next context switch to the process
 * if signal available.
 */
int signal(pid_t pid, int signalNumber) {
    pcb_t* process = pid_to_pcb(pid);
    if (process == NULL) {
        return SYSPID_DNE; 
    } 
    
    if (signalNumber < 0 || signalNumber >= SIGNAL_TABLE_SIZE) {
        return INVALID_SIGNAL; 
    }

    SET_BIT(process->signals_enabled, signalNumber);

    // Start pushing all the relevant arguments onto the stack 
    int *stack_ptr = (int*)process->esp;

    // save the return value
    stack_ptr = (int*)(((int)stack_ptr) - sizeof(int));
    *stack_ptr = process->ret;

    // push current context onto the stack
    stack_ptr -= 1;
    *stack_ptr = (int)process->esp;

    // push handler on stack
    stack_ptr -= 1;
    *stack_ptr = (int)process->signal_table[signalNumber];

    // push a placeholder return address (this is never used)
    stack_ptr -= 1;
    *stack_ptr = 0xBADA555;
    
    // Load the new context frame onto the stack pointing to sigtramp
    process->esp = (void*)((int)stack_ptr - sizeof(context_frame_t));
    context_frame_t *new_context = process->esp;
    init_context_frame(new_context, (funcptr)(&sigtramp));

    return 0;
}

/**
 * Mark signal for delivery
 */
int set_pcb_signal(pcb_t *pcb, int signal) {
    ASSERT(pcb != NULL);

    if (signal < 0 || signal >= SIGNAL_TABLE_SIZE) {
        return SYSKILL_SIG_INVALID;
    }
    
    // Only set signal if the handler is not null (ie. ignored)
    if (pcb->signal_table[signal]) {
        SET_BIT(pcb->signals_pending, signal);

        // If process is blocked, we need to unblock it and set the return value accordingly
        if (pcb->state == PROC_STATE_BLOCKED) { 
            // Set the return value for specific blockers, and add them to the ready queue
            switch(pcb->blocked_status) {
                case BLOCKED_STATUS_SLEEP:
                    wake(pcb);
                    break;
                case BLOCKED_STATUS_RECEIVE:
                case BLOCKED_STATUS_SEND:
                    pcb->ret = BLOCKED_PROC_SIGNALED;
                    remove_pcb_from_blocked_queue(pcb);
                    add_pcb_to_ready_queue(pcb);
                    break;
                case BLOCKED_STATUS_WAITING:
                    remove_pcb_from_blocked_queue(pcb);
                    add_pcb_to_ready_queue(pcb);
                    break;
                default:
                    ASSERT(0);
            }
        }
    }

    return 0;
}

/**
 * Delivers the highest priority signal to the process
 */
void deliver_highest_priority_signal(pcb_t *pcb) {
    ASSERT(pcb->signals_pending != 0);

    // Look in the signal table for the highest non-null entry. Higher number == higher priority
    int highest_sig_found = SIGNAL_TABLE_SIZE - 1;
    while (CHECK_BIT(pcb->signals_pending, highest_sig_found) == 0) {
        highest_sig_found--;
    }
    
    if(pcb->signals_pending > pcb->signals_enabled) {
        // Clear pending bit once found
        CLEAR_BIT(pcb->signals_pending, highest_sig_found);
        signal(pcb->pid, highest_sig_found);
    }
}

