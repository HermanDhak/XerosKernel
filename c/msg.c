/* msg.c : messaging system
 *
 * Called from outside:
 * send() - send a message to the specified process
 * recv() - recieve a message from the specified process
 */

#include <xeroskernel.h>
#include <xeroslib.h>
#include <stdarg.h>
#include <pcb.h>

/* 
 * Send a message to the specified process
 */
int send(pcb_t *curr_proc, pcb_t *dest_proc, void *buffer, int buffer_len) {
    pcb_t *any_receiver = peek_any_receiver();
    bool exists = dest_proc != NULL && remove_pcb_from_blocked_queue(dest_proc);
    if(exists || any_receiver != NULL) {
        if(exists == FALSE) {
            dest_proc = any_receiver;
        }

        if(dest_proc->pid != 0 && dest_proc->blocked_id != curr_proc->pid) {
            add_pcb_to_blocked_queue(dest_proc);
            curr_proc->blocked_status = BLOCKED_STATUS_SEND;
            curr_proc->blocked_id = dest_proc->pid;
            add_pcb_to_blocked_queue(curr_proc);
            return BLOCKERR;
        }

        va_list args = (va_list)dest_proc->args;
        va_arg(args, int);
        void *dest_buffer = (void*)(va_arg(args, int));
        int dest_buffer_len = va_arg(args, int);

        int min_buffer_len = buffer_len > dest_buffer_len ? dest_buffer_len : buffer_len;
        blkcopy(dest_buffer, buffer, min_buffer_len);
        add_pcb_to_ready_queue(dest_proc);
        dest_proc->blocked_id = 0;
        dest_proc->ret = min_buffer_len;
        return min_buffer_len; 
    } else {
        curr_proc->blocked_status = BLOCKED_STATUS_SEND;
        curr_proc->blocked_id = dest_proc->pid;
        add_pcb_to_blocked_queue(curr_proc);
        return BLOCKERR;
    }
}

/*
 * Recieve a message from the specified process
 */
int recv(pcb_t *from_proc, pcb_t *curr_proc, void *buffer, int buffer_len) {
    bool exists = from_proc != NULL;
    if(!exists) {
       from_proc = peek_next_sender();
    }

    if(from_proc != NULL && remove_pcb_from_blocked_queue(from_proc)) {
        va_list args = (va_list)from_proc->args;
        va_arg(args, int);
        void *from_buffer = (void*)(va_arg(args, int));
        int from_buffer_len = va_arg(args, int);

        if(exists && from_proc->blocked_id != curr_proc->pid) {
            add_pcb_to_blocked_queue(from_proc);
            curr_proc->blocked_status = BLOCKED_STATUS_RECEIVE;
            curr_proc->blocked_id = from_proc->pid;
            add_pcb_to_blocked_queue(curr_proc);
            return BLOCKERR;
        }
       
        int min_buffer_len = buffer_len > from_buffer_len ? from_buffer_len : buffer_len;
        blkcopy(buffer, from_buffer, min_buffer_len);
        add_pcb_to_ready_queue(from_proc);
        from_proc->blocked_id = 0;
        from_proc->ret = min_buffer_len;
        return min_buffer_len;
    } else {
        curr_proc->blocked_status = BLOCKED_STATUS_RECEIVE;
        if(exists) {
            curr_proc->blocked_id = from_proc->pid;
        } else {
            curr_proc->blocked_id = 0;
        }
        add_pcb_to_blocked_queue(curr_proc);
        return BLOCKERR;
    }
}


