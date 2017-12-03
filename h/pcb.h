/* pcb.h : Process control block related structs and prototypes */

#include <xeroskernel.h>

/* Struct representing a pcb queue */
typedef struct pcb_queue {
    pcb_t *head;      /* The head of the queue */
    pcb_t *tail;      /* The tail of the queue */
    int size;         /* Size of the queue */
} pcb_queue_t;

/* function prototypes for the process queue */
pcb_queue_t *init_pcb_queue(void);
int pcb_size(pcb_queue_t *queue);
pcb_t *pcb_poll(pcb_queue_t *queue);
pcb_t *pcb_peek(pcb_queue_t *queue);
void pcb_offer(pcb_queue_t *queue, pcb_t *entry);
bool pcb_remove(pcb_queue_t *queue, pcb_t *entry);
void dump_pcb_queue(pcb_queue_t *queue);

/* function prototypes for calls related to pcb */
extern void initpcb(void);
extern void add_pcb_to_stopped_queue(pcb_t *pcb);
extern void add_pcb_to_ready_queue(pcb_t *pcb);
extern bool remove_pcb_from_ready_queue(pcb_t *pcb);
extern void add_pcb_to_blocked_queue(pcb_t *pcb);
extern bool remove_pcb_from_blocked_queue(pcb_t *pcb);
extern void add_pcb_to_sleep_queue(pcb_t *pcb);
extern bool remove_pcb_from_sleep_queue(pcb_t *pcb);
extern pcb_t *peek_pcb_from_sleep_queue(void);
extern pcb_t *peek_next_sender(void);
extern pcb_t *peek_any_receiver(void);
extern void unblock_pcb_waiting_for_pid(pid_t pid);
extern pcb_t *get_next_pcb(void);
extern pcb_t *get_free_pcb(void);
extern pcb_t *pid_to_pcb(pid_t pid);
extern void cleanup_pcb(pcb_t *pcb);

extern void dump_stopped_queue(void);
extern void dump_ready_queue(void);
extern void dump_blocked_queue(void);
extern void dump_sleep_queue(void);

extern int fill_processStatus(pcb_t *pcb, processStatuses *ps);
extern void deliver_highest_priority_signal(pcb_t *pcb);
extern int set_pcb_signal(pcb_t *pcb, int signal);
