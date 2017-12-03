/* kerneltest.h - contains all the test function prototypes for the kernel */

#ifndef KERNELTEST_H
#define KERNELTEST_H

void run_mem_tests(void);
void run_queue_tests(void);
void run_proc_tests(void);
void run_sendrecv_tests(void);
void run_preemption_tests(void);
void run_kill_tests(void);
void run_signal_tests(void);
void run_device_tests(void);

#endif

