/* xeroskernel.h - disable, enable, halt, restore, isodd, min, max */

#ifndef XEROSKERNEL_H
#define XEROSKERNEL_H

/* Symbolic constants used throughout Xinu */

typedef	char bool;        /* Boolean type                  */
typedef unsigned int size_t; /* Something that can hold the value of
                              * theoretical maximum number of bytes 
                              * addressable in this architecture. */
typedef unsigned int pid_t;  /* Typedef to represent process id's */

#define	FALSE   0       /* Boolean constants             */
#define	TRUE    1
#define	EMPTY   (-1)    /* an illegal gpq                */
#define	NULL    0       /* Null pointer for linked lists */
#define	NULLCH '\0'     /* The null character            */
#define DEBUG   1       /* Enable/disable extra debug logs    */

#define CREATE_FAILURE -1 /* Process creation failed    */

/* Universal return constants */

#define	OK            1         /* system call ok               */
#define	SYSERR       -1         /* system call failed           */
#define	EOF          -2         /* End-of-file (usu. from read)	*/
#define	TIMEOUT      -3         /* time out  (usu. recvtim)     */
#define	INTRMSG      -4         /* keyboard "intr" key pressed	*/
                                /*  (usu. defined as ^B)        */
#define	BLOCKERR     -5         /* non-blocking op would block  */

/* Additional constants */

#define PARAGRAPH_SIZE 0x10       /* 16kb memory alignment       */
#define PCB_TABLE_SIZE 32         /* Maximum number of processes */
#define SIGNAL_TABLE_SIZE 32      /* Maximum number of supported signals */
#define PID_MAX 32768             /* Maximum process id number */
#define PCB_MAX_FDS 4             /* Maximum number of devices to be opened */
#define DEFAULT_STACK_SIZE 8192   /* Default stack size to use for user processes */
#define IDLE_PROC_STACK_SIZE 2048 /* Stack size to use for idle process */
#define MS_PER_CLOCK_TICK 10      /* Milliseconds per clock tick */

/* dispatcher constants */

#define SYSCALL_INT_NUM 255     /* Interrupt number for syscalls */
#define TIMER_INT_NUM 32       /* Interrupt number for timer */
#define KEYBOARD_INT_NUM 33       /* Interrupt number for keyboard */

/* Syscall return constants */
#define SYSPID_DNE -1
#define SYSPID_SELF -2
#define SYSERR_OTHER -3
#define KILL_SIGNAL_NUM 31 
#define INVALID_SIGNAL -1
#define SYSHANDLER_NEWHANDLER_INVALID -2
#define SYSHANDLER_OLDHANDLER_INVALID -3
#define SYSKILL_TARGET_DNE -512
#define SYSKILL_SIG_INVALID -561

/* If a process is currently blocked on a syscall, and recieves a signal,
 * the process will be unblocked and the return value for the syscall will be this constant. */
#define BLOCKED_PROC_SIGNALED -99     


/* Functions defined by startup code */

void           bzero(void *base, int cnt);
void           bcopy(const void *src, void *dest, unsigned int n);
void           disable(void);
unsigned short getCS(void);
unsigned char  inb(unsigned int);
void           init8259(void);
int            kprintf(char * fmt, ...);
void           lidt(void);
void           outb(unsigned int, unsigned char);
void           set_evec(unsigned int xnum, unsigned long handler);

/* Macros used for debugging */

#if DEBUG
#define LOG(...) kprintf("%s:%d (%s): ", __FILE__, __LINE__, __FUNCTION__); kprintf(__VA_ARGS__)
#else
#define LOG(...) /* otherwise don't print anything */
#endif
#define ASSERT(x) if (!(x)) { LOG("Assertion failed!"); while(1); }
#define ASSERT_EQUAL(x, y) if (x != y) { LOG("Assertion failed... %d != %d", x, y); while(1); }

/* Bit mask macros */
#define SET_BIT(flag, bitNum) { flag |= (0x01 << bitNum); }
#define CLEAR_BIT(flag, bitNum) { flag &= ~(0x01 << bitNum); }
#define TOGGLE_BIT(flag, bitNum) { flag ^= (0x01 << bitNum); }
#define CHECK_BIT(flag, bitNum) (0x01 & (flag >> bitNum))

/* Typedef for function pointers both with and without args */

typedef void (*funcptr)(void);
typedef void (*funcptr_args)(void*);

typedef struct pcb pcb_t; /* Forward declare this for the devsw table */

/* Structs and enums */

/* Represent process state */
typedef enum proc_state {
    PROC_STATE_READY,
    PROC_STATE_STOPPED,
    PROC_STATE_RUNNING,
    PROC_STATE_BLOCKED
} proc_state_t;

typedef struct struct_ps processStatuses;
struct struct_ps {
    int entries;                  // Last entry used in the table
    int pid[PCB_TABLE_SIZE];      // The process ID
    int status[PCB_TABLE_SIZE];   // The process status
    long cpuTime[PCB_TABLE_SIZE]; // CPU time used in milliseconds
};

/* Represent blocked status */
typedef enum blocked_status {
    BLOCKED_STATUS_NONE,
    BLOCKED_STATUS_SEND,
    BLOCKED_STATUS_RECEIVE,
    BLOCKED_STATUS_WAITING,
    BLOCKED_STATUS_SLEEP,
    BLOCKED_STATUS_DEVICE
} blocked_status_t;

/* Device driver struct */
typedef struct devsw {
    int dvnum;
    char dvname[20];   // Limiting device name to max number of chars
    int (*dvinit)(void);
    int (*dvopen) (pcb_t *pcb, void *dvioblk);
    int (*dvclose)(pcb_t *pcb, void *dvioblk);
    int (*dvread) (pcb_t *pcb, void *dvioblk, void *buf, int buflen);
    int (*dvwrite)(pcb_t *pcb, void *dvioblk, void *buf, int buflen);
    int (*dvioctl)(pcb_t *pcb, void *dvioblk, unsigned long command, void *args);
    int (*dviint)(void); // input available interrupt
    int (*dvoint)(void); // output available interrupt
    void *dvioblk; // device specific data (eg. pointer to another struct)
    int dvminor;
} devsw_t;

/* Device id enum */
typedef enum dev_id {
    DEV_ID_KEYBOARD_NO_ECHO = 0,
    DEV_ID_KEYBOARD,
    NUM_DEVICES
} dev_id_t;

/* Keyboard constants */
#define KEYBOARD_PORT_DATA_READ 0x60
#define KEYBOARD_PORT_CONTROL 0x64
#define KEYBOARD_IOCTL_ENABLE_ECHO 56
#define KEYBOARD_IOCTL_DISABLE_ECHO 55
#define KEYBOARD_IOCTL_SET_EOF 53

/* Struct describing a process control block */
typedef struct pcb {
    pid_t pid;           /* The PID of the process */
    proc_state_t state;  /* Current state of process */
    pid_t blocked_id;    /* the PID of what this PCB is waiting for */
    blocked_status_t blocked_status; /* Status of which it is blocked */
    struct pcb *next;    /* Pointer to next PCB */
    
    void *stack_start;   /* The start of the stack memory allocated to this pcb */
    void *esp;           /* Current location of the stack pointer */
    int ret;             /* Return value of the process */
    int cpu_time;        /* Total time this process has executed for */
    long args;           /* Syscall arguments */

    /* Signals */
    funcptr_args *signal_table;  /* pointer to the signal table */
    unsigned int signals_enabled;         /* signals currently enabled for this process */
    unsigned int signals_pending;         /* signals pending to be processed by this process */

    devsw_t *fd_table[PCB_MAX_FDS]; /* pointer to the file descriptor table */
} pcb_t;

/* Context frame representing the context of a process */
typedef struct context_frame {
    unsigned long edi;
    unsigned long esi;
    unsigned long ebp;
    unsigned long esp;
    unsigned long ebx;
    unsigned long edx;
    unsigned long ecx;
    unsigned long eax;
    unsigned long iret_eip;
    unsigned long iret_cs;
    unsigned long eflags;
} context_frame_t;

/* enums for different types of syscalls */
typedef enum request {
    SYSCALL_CREATE,
    SYSCALL_YIELD,
    SYSCALL_STOP,
    SYSCALL_GETPID,
    SYSCALL_PUTS,
    SYSCALL_KILL,
    SYSCALL_RECV,
    SYSCALL_SEND,
    SYSCALL_SLEEP,
    SYSCALL_CPUTIMES,
    SYSCALL_SIGHANDLER,
    SYSCALL_SIGRETURN,
    SYSCALL_WAIT,
    SYSCALL_OPEN,
    SYSCALL_CLOSE,
    SYSCALL_WRITE,
    SYSCALL_READ,
    SYSCALL_IOCTL,
    TIMER_INT,
    KEYBOARD_INT
} syscall_request_t;

/* Memory manager functions */

extern void kmeminit(void);
extern void *kmalloc(size_t size);
extern int kfree(void *ptr);
extern void mem_dump(void);
extern int verify_sysptr(void *ptr, long len);

/* Dispatcher functions */

extern void dispinit(void);
extern void dispatch(void);
extern void *next(void);
extern void ready(void *proc);

/* Context switching functions */

extern void contextinit(void);
extern syscall_request_t contextswitch(pcb_t *proc);

/* System call functions */

extern int syscall(int call, ...);
extern unsigned int syscreate(void(*func)(void), int stack);
extern void sysyield(void);
extern void sysstop(void);
extern pid_t sysgetpid(void);
extern void sysputs(char *str);
extern int syskill(pid_t pid, int signalNumber);
extern int syssend(pid_t dest_pid, void *buffer, int buffer_len);
extern int sysrecv(pid_t *from_pid, void *buffer, int buffer_len);
extern unsigned int syssleep(unsigned int milliseconds);
extern int sysgetcputimes(processStatuses *ps);
extern int syssighandler(int signal, funcptr_args newHandler, funcptr_args *oldHandler);
extern void syssigreturn(void *old_sp);
extern int syswait(pid_t pid);
extern int sysopen(int device_no);
extern int sysclose(int fd);
extern int syswrite(int fd, void *buff, int bufflen);
extern int sysread(int fd, void *buff, int bufflen);
extern int sysioctl(int fd, unsigned long command, ...);

/* Device independant functions (used by disp) */

extern int di_open(pcb_t *pcb, int device_no);
extern int di_close(pcb_t *pcb, int fd);
extern int di_write(pcb_t *pcb, int fd, void *buff, int bufflen);
extern int di_read(pcb_t *pcb, int fd, void *buff, int bufflen);
extern int di_ioctl(pcb_t *pcb, int fd, unsigned long command, void *args);
extern void di_init_devtable(void);

/* Creating processes functions */

extern int create(funcptr fp, int stack);
extern void init_context_frame(context_frame_t *context, funcptr fp);
extern void root(void);
extern int idleinit(void);
extern void idleproc(void);
extern pcb_t *get_idleproc(void);

/* Inter-process communication functions */

extern int send(pcb_t *curr_proc, pcb_t *dest_proc, void *buffer, int buffer_len);
extern int recv(pcb_t *from_pid, pcb_t *curr_proc, void *buffer, int buffer_len);

/* Signal handler functions */

extern void sigtramp(funcptr_args handler, void *cntx);
extern int signal(pid_t pid, int signalNumber);

/* Sleep process */

extern void sleepinit(void);
extern void sleep(pcb_t *pcb, unsigned int milliseconds);
extern void tick(void);
extern void wake(pcb_t *pcb);

#endif
