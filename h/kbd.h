/* kbd.h: Keyboard driver prototypes */

#include <xeroskernel.h>

// Upper half functions
void kbd_devsw_init(devsw_t *dev_entry, int echo);
int kbd_init(void);
int kbd_open(pcb_t *pcb, void *dvioblk);
int kbd_close(pcb_t *pcb, void *dvioblk);
int kbd_read(pcb_t *pcb, void *dvioblk, void* buff, int bufflen);
int kbd_write(pcb_t *pcb, void *dvioblk, void* buff, int bufflen);
int kbd_ioctl(pcb_t *pcb, void *dvioblk, unsigned long command, void *args);
int kbd_iint(void);
int kbd_oint(void);

// Lower half keyboard function
void keyboard_isr(void);
