
/* kbd.c: Keyboard device code
 * kbd_devsw_init() - Fills in a device table entry with keyboard-device specific values
 * kbd_init() - keyboard implementation for init
 * kbd_open() - keyboard implementation for open
 * kbd_close() - keyboard implementation for close
 * kbd_read() - keyboard implementation for read
 * kbd_write() - keyboard implementation for write
 * kbd_ioctl() - keyboard implementation for ioctl
 * keyboard_isr() - Function called when a keyboard interrupt occurs
 */

#include <xeroslib.h>
#include <kbd.h>
#include <pcb.h>
#include <i386.h>
#include <stdarg.h>

#define KBD_DEFAULT_EOF ((char)0x04)

#define KEYBOARD_PORT_CONTROL_READY_MASK 0x01
#define KEYBOARD_PORT_DATA_SCANCODE_MASK 0x0FF

typedef struct kbd_task {
    pcb_t *pcb;
    void *buff;
    int bufflen;
    int buff_count;
    int waiting_on_read;
} kbd_task_t;


// PID_based task list, using refcount to indicate when nobody is waiting
#define KBD_TASK_LIST_SIZE (PCB_TABLE_SIZE)
static kbd_task_t kbd_task_list[KBD_TASK_LIST_SIZE];
static int kbd_task_refcount = 0;

typedef struct kbd_dvioblk {
    int orig_echo_flag;
} kbd_dvioblk_t;

static int kbd_ioctl_set_eof(void *args);
// Only 1 keyboard type is allowed to be open at a time
static int kbd_refcount = 0;
static int kbd_current_type = 0; // Current type of keyboard opened
static int kbd_done = 0;

static void keyboard_flush_buffer(void);
static char keyboard_process_scancode(int data);
static void keyboard_unblock_proc(kbd_task_t *task);
static void keyboard_process_char(char c);
static void keyboard_handle_eof(void);

#define KEYBOARD_STATE_SHIFT_BIT 0
#define KEYBOARD_STATE_CTRL_BIT 1
#define KEYBOARD_STATE_CAPLOCK_BIT 2
static int keyboard_keystate_flag = 0;

// Circular buffer implementation, An extra index is used to indicate full buffer
#define KEYBOARD_BUFFER_SIZE (4 + 1)
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE] = {0};
static int keyboard_buffer_head = 0;
static int keyboard_buffer_tail = 0;
static char keyboard_eof;
static char keyboard_echo_flag; // 1 for on, 0 for off

/**
 * Fills in a device table entry with keyboard specific function.
 * Also set the keyboard type to echo characters automatically or not with
 * the echo flag.
 */
void kbd_devsw_init(devsw_t *table_entry, int echo) {
    ASSERT(table_entry != NULL);
    
    sprintf(table_entry->dvname, "keyboard");
    table_entry->dvinit = &kbd_init;
    table_entry->dvopen = &kbd_open;
    table_entry->dvclose = &kbd_close;
    table_entry->dvread = &kbd_read;
    table_entry->dvwrite = &kbd_write;
    table_entry->dvioctl = &kbd_ioctl;
    table_entry->dviint = &kbd_iint;
    table_entry->dvoint = &kbd_oint;
    table_entry->dvminor = echo;
    // Note: this kmalloc will intentionally never be kfree'd
    table_entry->dvioblk = (kbd_dvioblk_t*)kmalloc(sizeof(kbd_dvioblk_t));
    ASSERT(table_entry->dvioblk != NULL);
    ((kbd_dvioblk_t*)table_entry->dvioblk)->orig_echo_flag = echo;
}

/*
 * keyboard implementation for init
 */
int kbd_init(void) {
    int i;
    
    kbd_refcount = 0;
    kbd_done = 0;
    keyboard_buffer_head = 0;
    keyboard_buffer_tail = 0;
    kbd_task_refcount = 0;
    
    for (i = 0; i < KBD_TASK_LIST_SIZE; i++) {
        kbd_task_list[i].waiting_on_read = 0;
    }
    
    // Read data from the ports in case some interrupts were triggered in the past
    inb(KEYBOARD_PORT_DATA_READ);
    inb(KEYBOARD_PORT_CONTROL);
    return 0;
}

/*
 * keyboard implementation for open
 */
int kbd_open(pcb_t *pcb, void *dvioblk) {
    int echo_flag = ((kbd_dvioblk_t*)dvioblk)->orig_echo_flag;
   
   // Device has already been opened 
    if (kbd_refcount > 0) {
        if (kbd_current_type != echo_flag) {
            return SYSERR;
        }
        
        kbd_refcount++;
        return 0;
    }
    
    // Setup all all initial management params on open
    kbd_current_type = echo_flag;
    kbd_done = 0;
    keyboard_buffer_head = 0;
    keyboard_buffer_tail = 0;
    keyboard_keystate_flag = 0;
    keyboard_eof = KBD_DEFAULT_EOF;
    keyboard_echo_flag = echo_flag;
    kbd_refcount = 1;
    
    ASSERT(kbd_task_list[pcb->pid % KBD_TASK_LIST_SIZE].waiting_on_read == 0);
    // Enable interrupt handling for this device on open
    setKbdInt(1);
    return 0;
}

/*
 * keyboard implementation for close
 */
int kbd_close(pcb_t *pcb, void *dvioblk) {
    // unused var warning
    (void)dvioblk;
    
    // There should be 1 reference
    if (kbd_refcount <= 0) {
        return SYSERR;
    }
    
    kbd_refcount--;
    if (kbd_refcount == 0) {
        // Disable keyboard interrupts if there are no open fd's
        setKbdInt(0);
    }
    
    if (kbd_task_list[pcb->pid % KBD_TASK_LIST_SIZE].waiting_on_read) {
        kbd_task_refcount--;
    }
    
    kbd_task_list[pcb->pid % KBD_TASK_LIST_SIZE].waiting_on_read = 0;
    return 0;
}


/*
 * keyboard implementation for read
 */
int kbd_read(pcb_t *pcb, void *dvioblk, void* buff, int bufflen) {
    // remove unused var warnings
    (void)pcb;
    (void)dvioblk;
    
    kbd_task_t *task = &kbd_task_list[pcb->pid % KBD_TASK_LIST_SIZE];
    kbd_task_refcount++;
    
    task->pcb = pcb;
    task->buff = buff;
    task->buff_count = 0;
    task->bufflen = bufflen;
    task->waiting_on_read = 1;
    
    keyboard_flush_buffer();
    if (task->buff_count == bufflen) {
        return bufflen;
    }
    
    if (kbd_done) {
        // EOF was encountered. We have to do this check
        // here in case our buffer has a couple of stray \n characters,
        // which would require multiple reads to fully flush
        return task->buff_count;
    }
    
    // Block this process as we have not reached EOF yet 
    return BLOCKERR;
}

/**
 * keyboard implementation for write
 * We can't write to keyboard, so return -1 everytime
 */
int kbd_write(pcb_t *pcb, void *dvioblk, void* buff, int bufflen) {
    // Remove unused warning
    (void)pcb;
    (void)dvioblk;
    (void)buff;
    (void)bufflen;

    return -1;
}

/*
 * keyboard implementation for ioctl
 */
int kbd_ioctl(pcb_t *pcb, void *dvioblk, unsigned long command, void *args) {
    // Remove unused warning
    (void)pcb;
    (void)dvioblk;

    switch(command) {
        case KEYBOARD_IOCTL_SET_EOF:
            return kbd_ioctl_set_eof(args);

        case KEYBOARD_IOCTL_ENABLE_ECHO:
            keyboard_echo_flag = 1;
            return 0;

        case KEYBOARD_IOCTL_DISABLE_ECHO:
            keyboard_echo_flag = 0;
            return 0;

        default:
            return SYSERR;
    }
}


int kbd_oint(void) {
    return -1;
}

int kbd_iint(void) {
    return -1;
}


/**
 * Helper function for setting the specific EOF character to be recognized
 * Return 0 on success or an error code otherwise
 */
static int kbd_ioctl_set_eof(void *args) {
    va_list arg_list;
    
    if (args == NULL) {
        return SYSERR;
    }
    
    arg_list = (va_list)args;
    keyboard_eof = (char)va_arg(arg_list, int);
    
    return 0;
}

/* Lower half keyboard functions */

/**
 * Called when a keyboard interrupt occurs.
 * Reads the data from the keyboard's registers via ports and handles the data.
 * Write data to a waiting proc's buffer or be buffered internally in keyboard_buffer
 * to wait for future reads.
 */
void keyboard_isr(void) {
    int is_data_present;
    int data;
    char c = 0;
    
    is_data_present = KEYBOARD_PORT_CONTROL_READY_MASK & inb(KEYBOARD_PORT_CONTROL);
    data = KEYBOARD_PORT_DATA_SCANCODE_MASK & inb(KEYBOARD_PORT_DATA_READ);
    
    if (is_data_present) {
        c = keyboard_process_scancode(data);
        
        if (c != 0) {
            if (kbd_task_refcount > 0) {
                // If there is a task waiting, write to task
                keyboard_process_char(c);
            } else if (((keyboard_buffer_head + 1) % KEYBOARD_BUFFER_SIZE) != keyboard_buffer_tail) {
                // Make sure buffer has room
                keyboard_buffer[keyboard_buffer_head] = c;
                keyboard_buffer_head = (keyboard_buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
                if (keyboard_echo_flag && c != keyboard_eof) {
                    kprintf("%c", c);
                }
            }
        }
    }
}

/**
 * Flush the entire keyboard buffer into the buffers held by blocked processes
 */
static void keyboard_flush_buffer(void) {
    char c;
    
    // While the buffer still has some contents in it
    while (kbd_task_refcount > 0 &&
        keyboard_buffer_tail != keyboard_buffer_head) {
            
        c = keyboard_buffer[keyboard_buffer_tail];
        keyboard_process_char(c);
        keyboard_buffer_tail = (keyboard_buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    }
}

/**
 * handles ascii characters such as handling EOFs and newlines
 */
static void keyboard_process_char(char c) {
    int i;
    kbd_task_t *task;
    
    if (c == keyboard_eof) {
        keyboard_handle_eof();
        return;
    }
    
    for (i = 0; i < KBD_TASK_LIST_SIZE; i++) {
        if (kbd_task_list[i].waiting_on_read) {
            task = &kbd_task_list[i];
            ((char*)(task->buff))[task->buff_count] = c;
            task->buff_count++;
            if (keyboard_echo_flag && c != keyboard_eof) {
                kprintf("%c", c);
            }
            // Unblock once we've filled the buffer, OR  we encounter \n
            if (task->buff_count == task->bufflen || c == '\n') {
                keyboard_unblock_proc(task);
            }
        }
    }
}

/**
 * Handles EOF character which involves waking all blocked processes.
 */
static void keyboard_handle_eof(void) {
    int i;
    kbd_task_t *task;
 
    // Disable keyboard interrupts   
    setKbdInt(0);
    kbd_done = 1;
    
    // Flush all queues
    for (i = 0; i < KBD_TASK_LIST_SIZE; i++) {
        if (kbd_task_list[i].waiting_on_read) {
            task = &kbd_task_list[i];
            keyboard_unblock_proc(task);
        }
    }    
}

/**
 * keyboard_unblock_proc
 * Helper method to unblock a process
 * @param pcb - proc to unblock
 * @param retval - return value to set in the pcb
 */
static void keyboard_unblock_proc(kbd_task_t *task) {
    ASSERT(task != NULL);
    kbd_task_refcount--;
    task->waiting_on_read = 0;
    task->pcb->ret = task->buff_count;
   
    // Make sure we only unblock BLOCKED processes
    if (task->pcb->state == PROC_STATE_BLOCKED) {
        add_pcb_to_ready_queue(task->pcb);
        task->pcb->blocked_status = BLOCKED_STATUS_NONE;
    }
}

/**
 * Translate the scancodes given the raw data
 * Return the ascii character
 */
static char keyboard_process_scancode(int data) {
    static char lower[0x54] = {
        // 0x00 - 0x07
           0, 0x1B,  '1',  '2',  '3',  '4',  '5',  '6',
         '7',  '8',  '9',  '0',  '-',  '=', 0x08, '\t',
         'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
         'o',  'p',  '[',  ']', '\n',    0,  'a',  's',
         'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
        '\'',  '`',    0, '\\',  'z',  'x',  'c',  'v',
         'b',  'n',  'm',  ',',  '.',  '/',    0, 0x2A,
           0,  ' ',    0,    0,    0,    0,    0,    0,
           0,    0,    0,    0,    0,    0,    0,    0,
           0,    0, 0x2D,    0,    0,    0, 0x2B,    0,
        // 0x50 - 0x53
           0,    0,    0,    0
    };

    static char upper[0x54] = {
        // 0x00 - 0x07
           0, 0x1B,  '!',  '@',  '#',  '$',  '%',  '^',
         '&',  '*',  '(',  ')',  '_',  '+', 0x08, '\t',
         'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
         'O',  'P',  '{',  '}', '\n',    0,  'A',  'S',
         'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
         '"',  '~',    0,  '|',  'Z',  'X',  'C',  'V',
         'B',  'N',  'M',  '<',  '>',  '?',    0,    0,
           0,  ' ',    0,    0,    0,    0,    0,    0,
           0,    0,    0,    0,    0,    0,    0, 0x37,
        0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
        // 0x50 - 0x53
        0x32, 0x33, 0x30, 0x2E
    };
    
    static char ctrl[0x54] = {
        // 0x00 - 0x07
           0, 0x1B,    0,    0,    0,    0,    0, 0x1E,
           0,    0,    0,    0, 0x1F,    0, 0x7F,    0,
        0x11, 0x17, 0x05, 0x12, 0x14, 0x19, 0x15, 0x09,
        0x0F, 0x10, 0x1B, 0x1D, 0x0A,    0, 0x01, 0x13,
        0x04, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,    0,
           0,    0,    0, 0x1C, 0x1A, 0x18, 0x03, 0x16,
        0x02, 0x0E, 0x0D,    0,    0,    0,    0, 0x10,
           0,  ' ',    0,    0,    0,    0,    0,    0,
           0,    0,    0,    0,    0,    0,    0,    0,
           0,    0,    0,    0,    0,    0,    0,    0,
        // 0x50 - 0x53
           0,    0,    0,    0
    };
    
    char c = 0;
    
    if (data < 0x54) {
        if (CHECK_BIT(keyboard_keystate_flag, KEYBOARD_STATE_CTRL_BIT)) {
            c = ctrl[data];
        } else if (CHECK_BIT(keyboard_keystate_flag, KEYBOARD_STATE_SHIFT_BIT) ^
            CHECK_BIT(keyboard_keystate_flag, KEYBOARD_STATE_CAPLOCK_BIT)) {
            // XOR condition, either SHIFT is pressed, or CAPLOCK is on
            c = upper[data];
        } else {
            c = lower[data];
        }
    }
    
    if (c == 0) {
        switch(data) {
            case 0x2A:
            case 0x36:
                // shift key pressed
                SET_BIT(keyboard_keystate_flag, KEYBOARD_STATE_SHIFT_BIT);
                break;
            case 0xAA:
            case 0xB6:
                // shift key released
                CLEAR_BIT(keyboard_keystate_flag, KEYBOARD_STATE_SHIFT_BIT);
                break;
            case 0x1D:
                // ctrl key pressed
                SET_BIT(keyboard_keystate_flag, KEYBOARD_STATE_CTRL_BIT);
                break;
            case 0x9D:
                // ctrl key released
                CLEAR_BIT(keyboard_keystate_flag, KEYBOARD_STATE_CTRL_BIT);
                keyboard_keystate_flag &= ~KEYBOARD_STATE_CTRL_BIT;
                break;
            case 0x3A:
                // caplock pressed
                TOGGLE_BIT(keyboard_keystate_flag, KEYBOARD_STATE_CAPLOCK_BIT);
                break;
            default:
                break;
        }
    }
    return c;
}

