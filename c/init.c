/* initialize.c - initproc */

#include <i386.h>
#include <xeroskernel.h>
#include <xeroslib.h>
#include <kerneltest.h>

extern	int	entry( void );  /* start of kernel image, use &start    */
extern	int	end( void );    /* end of kernel image, use &end        */
extern  long	freemem; 	/* start of free memory (set in i386.c) */
extern char	*maxaddr;	/* max memory address (set in i386.c)	*/

/************************************************************************/
/***				NOTE:				      ***/
/***								      ***/
/***   This is where the system begins after the C environment has    ***/
/***   been established.  Interrupts are initially DISABLED.  The     ***/
/***   interrupt table has been initialized with a default handler    ***/
/***								      ***/
/***								      ***/
/************************************************************************/

/*------------------------------------------------------------------------
 *  The init process, this is where it all begins...
 *------------------------------------------------------------------------
 */

void rootinit(void);

void initproc( void )				/* The beginning */
{
    kprintf( "\n\nCPSC 415, 2017W1 \n32 Bit Xeros -1.0.0 - even before beta \nLocated at: %x to %x\n\n", &entry, &end); 
  
    kmeminit();
    kprintf("Memory manager initialized!\n");
    
    di_init_devtable();
    kprintf("Devices initialized!\n");

    contextinit();
    kprintf("Context switcher initialized!\n");

    dispinit();
    kprintf("Dispatcher initialized!\n\n");

    idleinit();

    /* TESTS: uncomment these and run as necessary */
    //run_mem_tests();
    //run_queue_tests();
    //run_proc_tests();
    //run_sendrecv_tests();
	//run_preemption_tests();
	//run_kill_tests();
    //run_signal_tests();
    //run_device_tests();

    rootinit();
    initPIT(100);
    kprintf("System initialization complete! Entering dispatcher...\n");
    dispatch();

    /* We should never reach this */
    kprintf("\n\nKernel panic! Please reboot the machine. \n");
    for(;;);
}

/**
 * Initializes the root program
 */
void rootinit(void) {
    kprintf("Launching root process...\n");
    create(root, DEFAULT_STACK_SIZE);
}
