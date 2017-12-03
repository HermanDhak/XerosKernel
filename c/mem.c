/* mem.c : memory manager 
 *
 * Called from outside:
 *  kmeminit() - Initialize the memory manager
 *  kmalloc()  - Allocate a chunk of memory. Returns a pointer to starting location
 *  kfree()    - Free memory block starting at provided pointer if it passes sanity check
 *  mem_dump_to_stdout() - Print out the free mem list to the screen 
 *  verify_sysptr() - Checks if pointer passed in syscall is valid
 */

#include <xeroskernel.h>
#include <i386.h>

extern long freemem;    /* start of free memory (set in i386.c) */
extern char *maxaddr;   /* max memory address (set in i386.c)  */

/* memory header is a node for the free list */
typedef struct mem_header {
   unsigned long size;
   struct mem_header *prev;
   struct mem_header *next;
   char *sanity_check;
   unsigned char data_start[];
} mem_header_t;

/* free list is a list of free chunks in memory */
static mem_header_t *free_list;

static size_t align_to_paragraph(size_t address);
static void coalesce_blocks(mem_header_t *first, mem_header_t *second);

/**
 * Initialize free memory list
 */
void kmeminit(void) {
    size_t memstart = align_to_paragraph(freemem);
    free_list = (mem_header_t *) memstart;
    free_list->size = (size_t)(HOLESTART - memstart);
    free_list->sanity_check = NULL;
    free_list->prev = NULL;
   
    /* Second node in free mem list after the hole */ 
    mem_header_t *second_region = (mem_header_t *) HOLEEND;
    second_region->size = (size_t)(maxaddr - HOLEEND) & ~(PARAGRAPH_SIZE - 1);
    second_region->prev = free_list;
    second_region->sanity_check = NULL;
    second_region->next = NULL;

    free_list->next = second_region;
    return;
}

/**
 * kmalloc dynamically allocates memory with size provided.
 * returns address if successfully allocated, else 0
 */
void *kmalloc(size_t size) {
    if (size <= 0) {
        return 0;
    }

    size_t required_chunk = align_to_paragraph(size) + sizeof(mem_header_t);
    mem_header_t *curr = free_list;

    while(curr != NULL) {
        if(required_chunk <= curr->size) {
            if(required_chunk != curr->size) {
                mem_header_t *leftover = (mem_header_t *)((size_t)curr + required_chunk);
                leftover->size = curr->size - required_chunk;
                leftover->prev = curr;
                leftover->next = curr->next;
                if(curr->next != NULL) {
                    curr->next->prev = leftover;
                }

                curr->next = leftover;
            }

            if(curr->prev) {
                curr->prev->next = curr->next;
            } else {
                free_list = curr->next;
            }

            curr->size = required_chunk;
            curr->next->prev = curr->prev;
            curr->sanity_check = (void*) curr->data_start;
            return curr->data_start;
        }

        curr = curr->next;
    }

    LOG("kmalloc unable to allocate requested memory: %d \n", size);
    return 0;
}

/**
 * kfree frees the dynamically allocated memory and returns it to the free list
 * returns 1 if success, else 0
 * */
int kfree(void *ptr) {
    mem_header_t *node = (mem_header_t *)(ptr - sizeof(mem_header_t));
    if(node->sanity_check != ptr) {
        return 0;
    }

    mem_header_t *prev = NULL;
    mem_header_t *next = free_list;
    while(next != NULL && next < node) {
        prev = next;
        next = next->next;
    }

    if(prev != NULL) {
        prev->next = node;
    } else {
        free_list = node;
    }

    if(next != NULL) {
        next->prev = node;
    }

    node->prev = prev;
    node->next = next;
    coalesce_blocks(node, next);
    coalesce_blocks(prev, node);
    return 1;
}

/**
 *  Aligns the given address to the 16 byte paragraph 
 */
size_t align_to_paragraph(size_t address) {
    if(address & (PARAGRAPH_SIZE - 1)) {
        return (address + PARAGRAPH_SIZE) & ~(PARAGRAPH_SIZE - 1);
    }

    return address;
}

/**
 * Combine two free mem blocks if they are next to each other.
 */
void coalesce_blocks(mem_header_t *first, mem_header_t *second) {
    if(first == NULL || second == NULL) {
        return;
    }
    
    if(first->data_start + first->size < second->data_start) {
        return;
    }

    first->size = second->data_start + second->size - first->data_start;
    first->next = second->next;
    if(second->next != NULL) {
        second->next->prev = first;
    }
}

/**
 * Dumps information about the free list to console
 */
void mem_dump(void) {
    mem_header_t *curr = free_list;
    while(curr != NULL) {
        kprintf("Start address: %ld Size: %ld\n", curr, curr->size);
        curr = curr->next;
    }

    kprintf("\n");
}


/**
 * Perform basic checks on the pointer passed in via a syscall to see if it's valid
 */
int verify_sysptr(void *ptr, long len) {
    // Is length in a valid range?
    ASSERT(len > 0);
    long addr = (long)ptr;
    long end_addr = addr + len - 1;

    // Is the address in a valid range?
    if (addr <= 0 || end_addr > (long)maxaddr) {
        return SYSERR;
    }
    
    // Is pointer within the hole?
    if ((addr >= HOLESTART && addr < HOLEEND) ||
        (end_addr >= HOLESTART && end_addr < HOLEEND)) {
        return SYSERR;
    }

    // Is pointer in the kernel stack?
    if (addr >= (freemem - KERNEL_STACK) && addr < freemem) {
        return SYSERR;
    }

    return OK;
}

