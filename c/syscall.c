/* syscall.c : syscalls 
 * 
 * Called from other processes:
 *   syscreate() - Create a new process
 *   sysyield()  - Pause execution of this process and allow another process to run
 *   sysstop()   - Stop the process 
 *   sysgetpid() - returns current process's pid
 *   sysputs() - allows processes to perform synchronized output
 *   syssleep() - Put process to sleep for a number of milliseconds
 *   syswait() - waits for a process to terminate
 *   sysgetcputimes() - Fills processStatuses struc with process cpu time info
 *   syssend() - sends data to a particular process
 *   sysrecv() - receives data delivered by syssend()
 *   syskill() - delivers a signal to a process
 *   syssighandler() - registers the handler as a signal handler
 *   syssigreturn() - restores a process's context after a signal is handled
 *   sysopen() - open a device
 *   sysclose() - close a file descriptor
 *   syswrite() - write to a file descriptor
 *   sysread() - read from a file descriptor
 *   sysioctl() - execute a device specific control command
*/

#include <xeroskernel.h>
#include <stdarg.h>

/**
 * Executes the requested syscall given a request id. The return value is stored
 * in %eax and then returned in a variable. Additional syscall arguments are stored
 * in %edx.
 */
int syscall(int req, ...) {
    va_list args;
    int result;

    va_start(args, req);

    __asm__ volatile( " \
        movl %1, %%eax \n\
        movl %2, %%edx \n\
        int %3  \n\
        movl %%eax, %0 \n"
        : "=r" (result)
        : "r" (req), "r" (args), "i" (SYSCALL_INT_NUM)
        : "%eax"
    );

    va_end(args);

    return result;
}

/**
 * Create a new process given a function pointer to the main function of new proc
 * and the stack size required
 */
unsigned int syscreate(void (*func)(void), int stack) {
    return (unsigned int)syscall(SYSCALL_CREATE,
                        (unsigned long)func, (unsigned long)stack);
}

/**
 * Pause execution of this process
 */
void sysyield(void) {
    syscall(SYSCALL_YIELD);
}

/**
 * Stop this process
 */
void sysstop(void) {
    syscall(SYSCALL_STOP);
}

/**
 * Return the PID of current process
 */
pid_t sysgetpid(void) {
    return syscall(SYSCALL_GETPID);
}

/**
 * Used by the process to perform synchronized output to the screen
 */
void sysputs(char *str) {
    syscall(SYSCALL_PUTS, str);
}

/**
 * Sends a signal to the process with the given pid.
 * Returns 0 on success and -1 on failure. 
 * A process can kill itself. Nothing is returned in that case.
 */
int syskill(pid_t pid, int signalNumber) {
    return syscall(SYSCALL_KILL, pid, signalNumber);
}

/**
 * Sends a message to another process
 */
int syssend(pid_t dest_pid, void *buffer, int buffer_len) {
    return syscall(SYSCALL_SEND, dest_pid, buffer, buffer_len);
}

/**
 * Receives a message from another process
 */
int sysrecv(pid_t *from_pid, void *buffer, int buffer_len) {
    return syscall(SYSCALL_RECV, from_pid, buffer, buffer_len);
}

/**
 * Makes the process sleep for int milliseconds
 */
unsigned int syssleep(unsigned int milliseconds) {
    return syscall(SYSCALL_SLEEP, milliseconds);
}

/**
 * Populates the processStatuses struct with the amount of time each process
 * has been running for as well as it's current status.
 */
int sysgetcputimes(processStatuses *ps) {
    return syscall(SYSCALL_CPUTIMES, ps);
}

/**
 * Causes the calling process to wait for the process specified with the pid to
 * terminate. Return 0 if process terminated, -1 is the process DNE
 */
int syswait(pid_t pid) {
    return syscall(SYSCALL_WAIT, pid);
}

/**
 * Registers the given function as the signal handler for the process.
 * Returns the old handler of the function through the given pointer.
 * If signal is invalid or is 31 return -1
 * If the handler is at an invalid address return -2
 * If successful return 0
 * If oldHandler address is invalid return -3
 */
int syssighandler(int signal, funcptr_args newHandler, funcptr_args *oldHandler) {
    return syscall(SYSCALL_SIGHANDLER, signal, newHandler, oldHandler);
}

/**
 * Restore a process's context after it is done handling a signal.
 * Does not return. Used by the signal trampoline code.
 */
void syssigreturn(void *old_sp) {
    syscall(SYSCALL_SIGRETURN, old_sp);
    ASSERT(0); // This should never get here, so add an assertion
}

/**
 * Open a device.
 * Return valid fd on success, -1 otherwise
 */
int sysopen(int device_no) {
    return syscall(SYSCALL_OPEN, device_no);
}

/** 
 * Closes a file descriptor.
 * Return 0 on success, -1 otherwise
 */
int sysclose(int fd) {
    return syscall(SYSCALL_CLOSE, fd);
}

/**
 * Write to a file descriptor
 * Return number of bytes written on success (Could be less then bufflen).
 * Return -1 on failure
 */
int syswrite(int fd, void *buff, int bufflen) {
    return syscall(SYSCALL_WRITE, fd, buff, bufflen);
}

/**
 * Read from a file descriptor
 * Return number of bytes read on success (Could be less then bufflen).
 * Return 0 for EOF.
 * Return -1 on failure.
 */
int sysread(int fd, void *buff, int bufflen) {
    return syscall(SYSCALL_READ, fd, buff, bufflen);
}

/**
 * Execute a specific control command on the given file descriptor.
 * Return 0 on success, -1 otherwise
 */
int sysioctl(int fd, unsigned long command, ...) {
    int result;
    va_list args;

    va_start(args, command);
    result = syscall(SYSCALL_IOCTL, fd, command, args);
    va_end(args);
    
    return result;
}


