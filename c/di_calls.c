/* di_calls.c: Device independant calls
 * di_init_devtable() - nitialize the device table as well as the devices in it
 * di_open() - device independent call for open 
 * di_close() - device independent call for close
 * di_write() - device independent call for write
 * di_read() - device independent call for read
 * di_ioctl() - device independent call for ioctl
 */

#include <xeroskernel.h>
#include <kbd.h>


static devsw_t dev_table[NUM_DEVICES];

static int valid_fd(pcb_t *pcb, int fd);

/**
 * Initialize the device table as well as the devices in it
 */
void di_init_devtable(void) {
    kbd_devsw_init(&dev_table[DEV_ID_KEYBOARD_NO_ECHO], 0);
    kbd_devsw_init(&dev_table[DEV_ID_KEYBOARD], 1);

    for (int i = 0; i < NUM_DEVICES; i++) {
        dev_table[i].dvinit();
    }
}

/*
 * device independent call for open
 */
int di_open(pcb_t *pcb, int device_no) {
    ASSERT(pcb != NULL);
    int fd;

    if (device_no < 0 || device_no >= NUM_DEVICES) {
        return SYSERR;
    }

    // Look for the first open fd slot (it will be null)
    for (fd = 0; fd < PCB_MAX_FDS; fd++) {
        if (pcb->fd_table[fd] == NULL) {
            break;
        }
    }

    if (fd >= PCB_MAX_FDS) {
        return SYSERR;
    }
    
    devsw_t *dev_entry = &dev_table[device_no];
    int result = dev_entry->dvopen(pcb, dev_entry->dvioblk);
    if (result) {
        return SYSERR;
    }

    // Mark this device table in the process' fd table
    pcb->fd_table[fd] = dev_entry;
    return fd;
}

/*
 * device independent call for close
 */
int di_close(pcb_t *pcb, int fd) {
    ASSERT(pcb != NULL);
    devsw_t *fd_entry;

    if (!valid_fd(pcb, fd)) {
        return SYSERR;
    }

    fd_entry = pcb->fd_table[fd];
    int result = fd_entry->dvclose(pcb, fd_entry->dvioblk);
    if (result) {
        return SYSERR;
    }

    // Clear the table entry 
    pcb->fd_table[fd] = NULL;
    return 0;
}

/*
 * device independent call for write
 */
int di_write(pcb_t *pcb, int fd, void *buff, int bufflen) {
    ASSERT(pcb != NULL);

    if (!valid_fd(pcb, fd)) {
        return SYSERR;
    }

    return pcb->fd_table[fd]->dvwrite(pcb, pcb->fd_table[fd]->dvioblk,
            buff, bufflen);
}

/*
 * device independent call for read
 */
int di_read(pcb_t *pcb, int fd, void *buff, int bufflen) {
    ASSERT(pcb != NULL);

    if (!valid_fd(pcb, fd)) {
        return SYSERR;
    }

    return pcb->fd_table[fd]->dvread(pcb, pcb->fd_table[fd]->dvioblk,
            buff, bufflen);
}

/*
 * device independent call for ioctl
 */
int di_ioctl(pcb_t *pcb, int fd, unsigned long command, void *command_args) {
    ASSERT(pcb != NULL);

    if (!valid_fd(pcb, fd)) {
        return SYSERR;
    }

    return pcb->fd_table[fd]->dvioctl(pcb, pcb->fd_table[fd]->dvioblk,
            command, command_args);
}


/**
 * Check to ensure fd is valid.
 * Return 1 if valid, and 0 if invalid
 */
static int valid_fd(pcb_t *pcb, int fd) {
    if (fd < 0 || fd >= PCB_MAX_FDS || pcb->fd_table[fd] == NULL) {
        return 0;
    } else {
        return 1;
    }
}
