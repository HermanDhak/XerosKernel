/* user.c : User processes
 */

#include <xeroskernel.h>
#include <xeroslib.h>
#include <pcb.h>

static void shell(void);
static int get_command(char *str, char *command, char* arg);
static void command_ps(void);
static void command_k(void);
static void command_a(void);
static void command_t(void);
static void filter_newline(char *str);

static int pid_to_kill;
static char *command_arg;

static char *detailed_states[] = {
    "READY",
    "STOPPED",
    "RUNNING",
    "BLOCKED: NONE",
    "BLOCKED: SENDING",
    "BLOCKED: RECEIVING",
    "BLOCKED: WAITING",
    "BLOCKED: SLEEPING",
    "BLOCKED: DEVICE IO"
};


/**
 * Authenticate the user, and start the shell process
 */
void root(void) {
    char* valid_user = "cs415";
    char* valid_pass = "EveryonegetsanA";

    while(1) {
        char user_buf[80];
        char pass_buf[80];
        memset(user_buf, '\0', sizeof(user_buf));
        memset(pass_buf, '\0', sizeof(pass_buf));

        sysputs("Welcome to Xeros - an experimental OS\n");
        // Open the keyboard device
        int fd = sysopen(DEV_ID_KEYBOARD);
        sysputs("Username: ");
        sysread(fd, user_buf, 20);

        sysioctl(fd, KEYBOARD_IOCTL_DISABLE_ECHO);

        sysputs("Password: ");
        sysread(fd, pass_buf, 20);
        sysclose(fd);

        filter_newline(user_buf);
        filter_newline(pass_buf);

        if (strcmp(user_buf, valid_user) == 0 &&
            strcmp(pass_buf, valid_pass) == 0) {
            int shell_pid = syscreate(&shell, DEFAULT_STACK_SIZE);
            syswait(shell_pid);
        } else {
            sysputs("\nInvalid username and/or password!\n\n");
        }

    }
}

/**
 * Converts first newline in a string to a null terminator
 */
static void filter_newline(char *str) {
    while(*str != '\0') {
        if (*str == '\n') {
            *str = '\0';
            break;
        }
        str++;
    }
}

/**
 * The user shell. Processes various commands
 */
static void shell(void) {
    sysputs("\n");
    int fd = sysopen(DEV_ID_KEYBOARD);

    char buf[100];

    while(1) {
        memset(buf, '\0', sizeof(buf));

        sysputs("> ");
        int bytes = sysread(fd, buf, 80);
        if (bytes == 0) {
            // EOF encountered
            break;
        }

        filter_newline(buf);
        char command[50];
        char arg[50];
        int ampersand = get_command(buf, command, arg);

        int wait = 1;
        int pid = 0;

        if(!strcmp("t", command)) {
            pid = syscreate(&command_t, DEFAULT_STACK_SIZE);
            wait = !ampersand;

        } else if(!strcmp("ps", command)) {
            pid = syscreate(&command_ps, DEFAULT_STACK_SIZE);

        } else if(!strcmp("a", command)) {
            command_arg = arg;
            pid = syscreate(&command_a, DEFAULT_STACK_SIZE);

        } else if(!strcmp("k", command)) {
            pid_to_kill = atoi(arg);
            pid = syscreate(&command_k, DEFAULT_STACK_SIZE);

        } else if(!strcmp("ex", command)) {           
            break;

        } else {
            sysputs("Invalid command!\n");
        }

        if (wait) {
            syswait(pid);
        }
    }

    sysputs("Logging out...\n");
    sysclose(fd);
}

/**
 * Parses user input to determine the command, and if & was passed
 * After parsing, the command is placed in the command buffer, and arg is
 * placed in arg.
 * Returns 0 if & included, 1 if it wasnt
 */
static int get_command(char* str, char* command, char* arg) {
    int ampersand = 0;
    int len = 0;

    while (*str != '\0' && *str != ' ' && *str != '&') {
        *command = *str;
        command++;
        str++;
        len++;
    }

    *command = '\0';

    // get to first argument
    while ((*str != '\0') && (*str == ' ' || *str == '&')) {
        str++;
        len++;
    }

    // read argument
    while (*str != '\0' && *str != ' ' && *str != '&') {
        *arg = *str;
        arg++;
        str++;
        len++;
    }

    *arg = '\0';

    // Look for a & at the end of the line
    while(*str != '\0') {
        str++;
        len++;
    }

    len--;
    str--; 
    while((len > 0) && (*str == ' ' || *str == '&')) {
        if (*str == '&') {
            ampersand = 1;
        }
        if (*str != ' ') {
            break;
        }
        str--;
        len--;
    }

    return ampersand;
}

/**
 * Lists all current active processes in a table
 */
static void command_ps(void) {
    processStatuses ps;
    char str[80];

    int num = sysgetcputimes(&ps);

    sysputs("PID | State           | Time\n");
    for (int i = 0; i <= num; i++) {
        sprintf(str, "%4d  %16s  %8d\n", ps.pid[i],
                detailed_states[ps.status[i]], ps.cpuTime[i]);
        sysputs(str);
    }
}

/**
 * Kills the process with pid pid_to_kill
 */
static void command_k(void) {
    if (pid_to_kill == 0) {
        sysputs("Cannot terminate idle proc.\n");
        return;
    }

    int ret = syskill(pid_to_kill, KILL_SIGNAL_NUM);
    if (ret) {
        sysputs("No such process.\n");
    }
}

/**
 * Signal handler for the a command
 */
static void command_a_handler(void) {
    funcptr_args oldHandler;
    
    sysputs("ALARM ALARM ALARM\n");
    syssighandler(15, NULL, &oldHandler);
}

/**
 * Installs the alarm signal handler which is run after the number of milliseconds
 * inputted to the function 
 */
static void command_a(void) {
    funcptr_args oldHandler;
    int sleeparg = atoi(command_arg);
    
    if (sleeparg <= 0) {
        sysputs("Usage: Enter SLEEP_MILLIS\n");
        return;
    }
    
    syssighandler(15, (funcptr_args)&command_a_handler, &oldHandler);
    syssleep(MS_PER_CLOCK_TICK * sleeparg);
    syskill(sysgetpid(), 15);
}

/**
 * Starts a process which prints out "T" every 10s forever
 */
static void command_t(void) {
    while(1) {
        sysputs("T\n");
        syssleep(10000);
    }
}

