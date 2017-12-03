/* ctsw.c : context switcher
 *
 * Called from outside:
 *  contextinit() - Initializes irq handlers that enable entry to the context switcher
 *  contextswitch() - Switch context from the kernel to the user process
 */

#include <xeroskernel.h>
#include <pcb.h>

#define CTSW_TYPE_SYSCALL 0
#define CTSW_TYPE_TIMER 1
#define CTSW_TYPE_KEYBOARD 2

void _syscall_entry_point(void);
void _timer_entry_point(void);
void _common_entry_point(void);
void _keyboard_entry_point(void);

static void *kern_stack_ptr;
static unsigned long *ESP;
static unsigned long rc;
static long args;
static int ctsw_type;
context_frame_t *cf;

/**
 * Sets the syscall interrupt handler
 */
void contextinit(void) {
    set_evec(SYSCALL_INT_NUM, (unsigned long)_syscall_entry_point);
    set_evec(TIMER_INT_NUM, (unsigned long)_timer_entry_point);
    set_evec(KEYBOARD_INT_NUM, (unsigned long)_keyboard_entry_point);
}

/*
 * Switches contexts from the kernel to the process provided.
 * Returns the ID of the syscall request
 */
syscall_request_t contextswitch(pcb_t *proc) {
    ASSERT(proc != NULL);

    /* Removes unused var compiler warning */
    (void)kern_stack_ptr;    
    
    /* If a pending signal is available and it has been enabled, deliver it */
    if (proc->signals_pending) {
        deliver_highest_priority_signal(proc);
    }

    ESP = proc->esp;
    rc = proc->ret;

    cf = (context_frame_t *)proc->esp;
    cf->eax = proc->ret;

    __asm__ volatile( " \
        pushf \n\
        pusha \n\
        movl    rc, %%eax    \n\
        movl    ESP, %%edx    \n\
        movl    %%esp, ESP    \n\
        movl    %%edx, %%esp \n\
        movl    %%eax, 28(%%esp) \n\
        popa \n\
        iret \n\
   _timer_entry_point: \n\
        cli \n\
        pusha  \n\
        movl $1, ctsw_type \n\
        jmp _common_entry_point \n\
    _keyboard_entry_point: \n\
        cli \n\
        pusha \n\
        movl $2, ctsw_type \n\
        jmp _common_entry_point \n\
    _syscall_entry_point: \n\
        cli \n\
        pusha  \n\
        movl $0, ctsw_type \n\
   _common_entry_point: \n\
        movl    %%eax, %%ebx \n\
        movl    ESP, %%eax  \n\
        movl    %%esp, ESP  \n\
        movl    %%eax, %%esp  \n\
        movl    %%ebx, 28(%%esp) \n\
        movl    %%edx, 20(%%esp) \n\
        popa \n\
        popf \n\
        movl    %%eax, rc \n\
        movl    %%edx, args \n\
        "
        : 
        : 
        : "%eax", "%ebx", "%edx"    );

    
    proc->esp = ESP;
    cf = (context_frame_t *)proc->esp;
    proc->ret = cf->eax;

    switch(ctsw_type) {
        case CTSW_TYPE_SYSCALL:
            proc->ret = rc;
            proc->args = args;
            break;
        case CTSW_TYPE_TIMER:
            rc = TIMER_INT;
            break;
        case CTSW_TYPE_KEYBOARD:
            rc = KEYBOARD_INT;
            break;
        default:
            kprintf("Unknown context switch type: %d. Halting kernel.\n", ctsw_type);
            while(1);

    }

    return rc;
}

