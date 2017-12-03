/* disp.c : dispatcher
 *
 * Called from outside:
 *  dispinit() - Initializes the dispatcher
 *  dispatch() - Fires up the dispatcher and enters an infinite loop
 */

#include <xeroskernel.h>
#include <pcb.h>
#include <stdarg.h>
#include <kbd.h>
#include <i386.h>

static int handle_syscall_create(void);
static void handle_syscall_puts(void);
static int handle_syscall_kill(void);
static int handle_syscall_send(void);
static int handle_syscall_recv(void);
static void handle_syscall_sleep(void);
static int handle_syscall_cputimes(void);
static void handle_syscall_wait(void);
static int handle_syscall_sighandler(void);
static void handle_syscall_sigreturn(void);
static void handle_syscall_open(void);
static void handle_syscall_close(void);
static void handle_syscall_write(void);
static void handle_syscall_read(void);
static void handle_syscall_ioctl(void);

static pcb_t *process;
static funcptr fp;
static va_list args;
static int ptr_check;
/**
 * Initialize the dispatcher and pcb related items
 */
void dispinit(void) {
    initpcb();
}

/**
 * Assuming a root process has been created before calling this function.
 * Dispatch manages context switching between processes and performing the required
 * actions as per their syscall request. This function never returns.
 */
void dispatch(void) {
    process = get_next_pcb();
    while(1) {
        if(process == NULL) {
            process = get_idleproc();
        }

        syscall_request_t request = contextswitch(process);
        switch(request) {
            case SYSCALL_CREATE:
                process->ret = handle_syscall_create();
                break;
            
            case SYSCALL_YIELD:
                add_pcb_to_ready_queue(process);
                process = get_next_pcb();
                break;
            
            case SYSCALL_STOP:
                cleanup_pcb(process);
                process = get_next_pcb();
                break;
            
            case SYSCALL_GETPID:
                process->ret = process->pid;
                break;
            
            case SYSCALL_KILL:
                process->ret = handle_syscall_kill();
                break;
            
            case SYSCALL_PUTS:
                handle_syscall_puts();
                break;
           
            case SYSCALL_SEND:
                process->ret = handle_syscall_send();
                break;
            
            case SYSCALL_RECV:
                process->ret = handle_syscall_recv();
                break;

            case SYSCALL_SLEEP:
                handle_syscall_sleep();
                process = get_next_pcb();
                break;
            
            case SYSCALL_CPUTIMES:
                process->ret = handle_syscall_cputimes();
                break;

            case SYSCALL_SIGHANDLER:
                process->ret = handle_syscall_sighandler();
                break;

            case SYSCALL_SIGRETURN:
                handle_syscall_sigreturn();
                break;

            case SYSCALL_WAIT:
                handle_syscall_wait();
                break;

            case SYSCALL_OPEN:
                handle_syscall_open();
                break;

            case SYSCALL_CLOSE:
                handle_syscall_close();
                break;
            
            case SYSCALL_WRITE:
                handle_syscall_write();
                break;

            case SYSCALL_READ:
                handle_syscall_read();
                break;

            case SYSCALL_IOCTL:
                handle_syscall_ioctl();
                break;

            case TIMER_INT:
                tick();
                process->cpu_time++;
                if(process->pid != 0) {
                    add_pcb_to_ready_queue(process);
                }
                process = get_next_pcb();
                end_of_intr();
                break;

            case KEYBOARD_INT:
                keyboard_isr();
                end_of_intr();
                break;

            default:
                kprintf("Invalid syscall request: %d, pid=%u. Halting kernel.",
                        request, process->pid);
                while(1);
        }
    }
}

/**
 * Handler for the create syscall. Returns 1 if create was successful, 
 * 0 otherwise.
 */
static int handle_syscall_create(void) {
    args = (va_list)process->args;
    fp = (funcptr)(va_arg(args, int));
    int size = va_arg(args, int);

    ptr_check = verify_sysptr(fp, sizeof(funcptr));
    if (ptr_check != OK) {
        return ptr_check;
    }
    return create(fp, size);
}

/**
 * Handler for the puts syscall. Prints the given string out to the screen.
 */
static void handle_syscall_puts(void) {
    args = (va_list)process->args;
    char *str = (char*)(va_arg(args, int));
    kprintf(str);
    process->ret = 0;
}

/** 
 * Handler for the kill syscall.
 * Returns 0 on success, and error codes on failure.
 */
static int handle_syscall_kill(void) {
    args = (va_list)process->args;
    pid_t pid = (pid_t)(va_arg(args, int));
    int signal = va_arg(args, int);

    pcb_t *proc_to_signal = pid_to_pcb(pid);

    if (proc_to_signal == NULL) {
        return SYSKILL_TARGET_DNE;
    }
    
    return set_pcb_signal(proc_to_signal, signal);
}

static void handle_syscall_wait(void) {
    args = (va_list)process->args;
    pid_t pid = (pid_t)va_arg(args, int);

    pcb_t *proc_to_wait_on = pid_to_pcb(pid);
    if (proc_to_wait_on == NULL || pid == 0) {
        process->ret = SYSPID_DNE;
        return;
    }
    
    // Add this process to the blocked queue as it waits on the targeted process to terminate.
    process->blocked_status = BLOCKED_STATUS_WAITING;
    process->blocked_id = pid;
    add_pcb_to_blocked_queue(process);

    // Setup return value to assume the target process was eventually killed
    process->ret = 0;
    process = get_next_pcb();
}

/** 
 * Handler for the send syscall. Returns -1 if pid does not exist,
 * -2 if send and recv pid is the same, and -3 otherwise.
 */
static int handle_syscall_send(void) { 
    args = (va_list)process->args;
    int dest_pid = va_arg(args, int);
    void *buffer = (void*)(va_arg(args, int));
    int buffer_len = va_arg(args, int);

    pcb_t *dest_proc = pid_to_pcb(dest_pid);
    if (dest_proc == NULL) {
        return SYSPID_DNE;
    }

    if (dest_pid == process->pid) {
        return SYSPID_SELF;
    }

    if (buffer_len <= 0 || verify_sysptr(buffer, buffer_len) != OK) {
        return SYSERR_OTHER;
    }

    int ret = send(process, dest_proc, buffer, buffer_len);
    if(ret == BLOCKERR) {
        process = get_next_pcb();
        return SYSPID_DNE;
    }
    return ret;
}

/** 
 * Handler for the recv syscall. Returns -1 if pid does not exist,
 * -2 if send and recv pid is the same, and -3 otherwise.
 */
static int handle_syscall_recv(void) { 
    args = (va_list)process->args;
    int *from_pid = va_arg(args, int);
    void *buffer = (void*)(va_arg(args, int));
    int buffer_len = va_arg(args, int);
    
    if (verify_sysptr(from_pid, sizeof(int)) != OK) {
        return SYSERR_OTHER;
    }

    pcb_t *from_proc = pid_to_pcb(*from_pid);
    if (*from_pid != 0 && from_proc == NULL) {
        return SYSPID_DNE;
    }

    if (*from_pid == process->pid) {
        return SYSPID_SELF;
    }

    if (buffer_len <= 0 || verify_sysptr(buffer, buffer_len) != OK) {
        return SYSERR_OTHER;
    }

    int ret = recv(from_proc, process, buffer, buffer_len);
    if(ret == BLOCKERR) {
        process = get_next_pcb();
        return SYSPID_DNE;
    }
 
    return ret;
}

/*
 * Handler for sleep syscall. Return 0 if sleep success,
 * otherwise returns remaining time.
 */
static void handle_syscall_sleep(void) {
    args = (va_list)process->args;
    unsigned int milliseconds = va_arg(args, int);
    if(milliseconds == 0) {
        process->ret = 0;
    }
    sleep(process, milliseconds);
}

extern char* maxaddr;

static int handle_syscall_cputimes() {
  args = (va_list)process->args;
  processStatuses *ps = va_arg(args, processStatuses *);
  

  // Check if address is in the hole
  if (((unsigned long) ps) >= HOLESTART && ((unsigned long) ps <= HOLEEND)) 
    return -1;

  //Check if address of the data structure is beyone the end of main memory
  if ((((char *) ps) + sizeof(processStatuses)) > maxaddr)  
    return -2;

  // There are probably other address checks that can be done, but this is OK for now

  return fill_processStatus(process, ps);
}

/**
 * Handler for syssighandler
 * Return 0 on success if handler installed, otherwise returns error codes.
 */
static int handle_syscall_sighandler(void) { 
    args = (va_list)process->args;
    int signal = va_arg(args, int);
    funcptr_args new_handler = va_arg(args, funcptr_args);
    funcptr_args *old_handler = (funcptr_args*)va_arg(args, funcptr_args);
    
    if (signal < 0 || signal >= SIGNAL_TABLE_SIZE || signal == KILL_SIGNAL_NUM) {
        return INVALID_SIGNAL;
    }
    
    // Do pointer checks on both handler addresses. New handler is allowed to be null
    if (new_handler != NULL && verify_sysptr(new_handler, sizeof(funcptr_args)) != OK) {
        return SYSHANDLER_NEWHANDLER_INVALID;
    }
    
    if (verify_sysptr(old_handler, sizeof(funcptr_args*)) != OK) {
        return SYSHANDLER_OLDHANDLER_INVALID;
    }

    // Capture old handler and set the new one
    *old_handler = process->signal_table[signal];
    process->signal_table[signal] = new_handler;

    return 0;
}

/**
 * Only used by the signal trampoline code! Replaces stack pointer of process
 * with pointer of old sp passed into this function. This doesn't return.
 */
static void handle_syscall_sigreturn(void) { 
    args = (va_list)process->args;
    void *old_sp = (void*)va_arg(args, int);

    // Ensure pointer is still valid. It could only become invalid is user modified their stack
    if (verify_sysptr(old_sp, sizeof(void*)) != OK) {
        cleanup_pcb(process); // kill process in this case
        process = get_next_pcb();
        return;
    }

    // Restore saved return value
    process->ret = *(int*)(((int)old_sp) - sizeof(int));
    process->esp = old_sp;

    int highest_sig_found = SIGNAL_TABLE_SIZE - 1;
    while (CHECK_BIT(process->signals_enabled, highest_sig_found) == 0) {
        highest_sig_found--;
    }

    CLEAR_BIT(process->signals_enabled, highest_sig_found);
} 

/**
 * Handler for sysopen
 * Return fd on success, -1 on failure
 */
static void handle_syscall_open(void) {
    args = (va_list)process->args;
    int device_no = va_arg(args, int);

    process->ret = di_open(process, device_no);
}

/**
 * Handler for sysclose
 * Return 0 on success, -1 on failure
 */
static void handle_syscall_close(void) { 
    args = (va_list)process->args;
    int fd = va_arg(args, int);
    
    process->ret = di_close(process, fd);
}

/**
 * Handler for sysread
 * Return number of bytes read on succes, -1 on failure
 */
static void handle_syscall_read(void) { 
    args = (va_list)process->args;
    int fd = va_arg(args, int);
    void *buff = (void*)va_arg(args, int);
    int bufflen = va_arg(args, int);

    if (verify_sysptr(buff, bufflen) != OK) {
        process->ret = SYSERR;
        return;
    }

    process->ret = di_read(process, fd, buff, bufflen);

    if (process->ret == BLOCKERR) {
        process->state = PROC_STATE_BLOCKED;
        process->blocked_status = BLOCKED_STATUS_DEVICE;
        process = get_next_pcb();
    }
}

/** 
 * Handler for syswrite
 * Return number of bytes written on succes, -1 on failure
 */ 
static void handle_syscall_write(void) {
    args = (va_list)process->args;
    int fd = va_arg(args, int);
    void *buff = (void*)va_arg(args, int);
    int bufflen = va_arg(args, int);

    if (verify_sysptr(buff, bufflen) != OK) {
        process->ret = SYSERR;
        return;
    }

    process->ret = di_write(process, fd, buff, bufflen);
}

/**
 * Handler for sysioctl
 * Return 0 on success, -1 on failure
 */
static void handle_syscall_ioctl(void) { 
    args = (va_list)process->args;
    int fd = va_arg(args, int);
    unsigned long command  = (unsigned long)(va_arg(args, long));

    void *command_args = (void*)(va_arg(args, int));

    process->ret = di_ioctl(process, fd, command, command_args);
}

