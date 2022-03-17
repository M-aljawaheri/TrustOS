/*
 * A dynamic memory allocator
 * Code by: Aman Haris
 * andrewId: asharis
 * 
 * Memory utilization on shark machine: 74%
 * Throughput on shark machine: ~15000-16000 Kops/sec
 * Note: I've been able to get this result consistently when the shark machine
 * is lighly loaded, but not at other times when it's busier.
 * 
 * Implementation details:
 * This code was written on top of the mm-baseline.c code provided 
 * in the handout.
 * 
 * Details:
 * 1) Free lists are handled as 40 segregated lists: the first 32 lists 
 *   correspond to the first 32 multiples of 16 (i.e. upto 512). This
 *   makes locating their list very efficient (seg_lists[size/16-1]). 
 *   The remaining 8 lists correspond to powers of 2 from 1024 to 8192+.
 * 2) The minimum block size is 16. Free blocks of size 32+ have a header,
 *   next pointer, prev pointer, and footer. Free blocks of size 16 have a 
 *   prev pointer stored where the header is, and a next pointer. All sizes
 *    when allocatted have 1 header and then the  payload only.
 * 3) Fit policy: first fit
 * 4) Coalasce policy: always coalesce
 * 5) Bit flags on header:
 *      1st lower-order bit for alloc.
 *      2nd bit for "if previous block is free", since we have no footers.
 *      3rd bit for "if previous block is size 16" since they have no footers.
 *      4th bit only checked on free blocks. If it is 1, that means that the 
 *          header contains a pointer to an 8 mod 16 aligned block, rather 
 *          than a size, from which we can deduce the size of the block as 
 *          16 (eliminating the the need for a header in 16-size free blocks).
 */

/* Do not change the following! */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stddef.h>

#include "mm.h"
#include "memlib.h"

#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* def DRIVER */

/* You can change anything from here onward */

/*
 * If DEBUG is defined, enable printing on dbg_printf and contracts.
 * Debugging macros, with names beginning "dbg_" are allowed.
 * You may not define any other macros having arguments.
 */
 //#define DEBUG // uncomment this line to enable debugging

#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disnabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

/* Basic constants */
typedef uint64_t word_t;
// word, header, footer size (bytes)
static const size_t wsize = sizeof(word_t);
// double word size (bytes)
static const size_t dsize = 2*wsize;        
// Minimum block size
static const size_t min_block_size = dsize; 
 // requires (chunksize % 16 == 0)
static const size_t chunksize = (1 << 11) - 16*26; 
 
typedef struct block
{
    /* Header contains size + allocation flag */
    word_t header;
    /*
     * We don't know how big the payload will be.  Declaring it as an
     * array of size 0 allows computing its starting address using
     * pointer notation.
     */
    union {
        char payload[0];
        struct dir {
            struct block *next;
            struct block *prev;
        } d;
    };
    /*
     * We can't declare the footer as part of the struct, since its starting
     * position is unknown
     */ 
} block_t;


/* Global variables */

#define NUM_SEG_LISTS 37

/* Pointer to first block */
static block_t *heap_listp = NULL;
static bool just_extended = false;
static block_t **seg_lists_gb;

/* Function prototypes for internal helper routines */
static block_t *extend_heap(size_t size);
static void place(block_t *block, size_t asize);
static block_t *find_fit(size_t asize);
static block_t *coalesce(block_t *block);

static size_t max(size_t x, size_t y);
static size_t round_up(size_t size, size_t n);
static word_t pack(size_t size, bool alloc);

static size_t extract_size(word_t header);
static size_t get_size(block_t *block);
static size_t get_payload_size(block_t *block);

static bool extract_alloc(word_t header);
static bool extract_prev(word_t header);
static bool extract_16(word_t header);
static bool get_alloc(block_t *block);
static bool get_prev(block_t *block);
static bool get_16(block_t *block);
static void set_prev(block_t *block);
static void set_16(block_t *block);
static void free_prev(block_t *block);
static void free_16(block_t *block);

static void write_header(block_t *block, size_t size, bool alloc);
static void write_footer(block_t *block, size_t size, bool alloc);

static block_t *payload_to_header(void *bp);
static void *header_to_payload(block_t *block);

static block_t *find_next(block_t *block);
static word_t *find_prev_footer(block_t *block);
static block_t *find_prev(block_t *block);

static void add_to_freelist(block_t *block);
static void remove_from_freelist(block_t *block);
static block_t **find_list(size_t size);
static size_t seglist_addr_to_idx(block_t **addr);
static bool no_loops();
static bool in_lists(block_t *block);

bool mm_checkheap(int lineno);

/*
 * mm_init: initializes the heap; it is run once when heap_start == NULL.
 * initializes in the following structure:
 *          INIT: SEG_LISTS | PROLOGUE_FOOTER | EPILOGUE_HEADER |
 * heap_listp ends up pointing to the epilogue header.
 */
bool mm_init(void) 
{
    // Create the initial empty heap 
    block_t **seg_lists;
    word_t *start;
    seg_lists = (block_t **)(mem_sbrk(40*wsize));
    int i;
    if (seg_lists == (void *)-1) 
    {
        return false;
    }
    seg_lists_gb= seg_lists;
    start = (word_t *)(seg_lists + 38);
    start[0] = pack(0, true); // Prologue footer
    start[1] = pack(0, true); // Epilogue header
    // Heap starts with first block header (epilogue)
    heap_listp = (block_t *) &(start[1]);
    for (i = 0; i < NUM_SEG_LISTS; i++)
    {
        seg_lists[i] = NULL;
    }

    // Extend the empty heap with a free block of chunksize bytes
    if (extend_heap(chunksize) == NULL)
    {
        return false;
    }
    return true;
}

/*
 * malloc: allocates a block with size at least (size + wsize), rounded up to
 *         the nearest 16 bytes, with a minimum of dsize. Seeks a
 *         sufficiently-large unallocated block on the heap to be allocated.
 *         If no such block is found, extends heap by the maximum between
 *         chunksize and (size + wsize) rounded up to the nearest 16 bytes,
 *         and then attempts to allocate all, or a part of, that memory.
 *         Returns NULL on failure, otherwise returns a pointer to such block.
 *         The allocated block will not be used for further allocations until
 *         freed.
 */
void *malloc(size_t size) 
{
    dbg_assert(mm_checkheap(212));

    size_t asize;      // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit is found
    block_t *block;
    void *bp = NULL;

    if (heap_listp == NULL) // Initialize heap if it isn't initialized
    {
        mm_init();
    }

    if (size == 0) // Ignore spurious request
    {
        return bp;
    }

    // Adjust block size to include overhead and to meet alignment requirements
    asize = round_up(size + wsize, dsize);

    // Search the free list for a fit
    block = find_fit(asize);

    // If no fit is found, request more memory, and then and place the block
    if (block == NULL)
    {  
        extendsize = max(asize, chunksize);
        block = extend_heap(extendsize);
        just_extended = true;
        if (block == NULL) // extend_heap returns an error
        {
            just_extended = false;
            return bp;
        }
    }

    place(block, asize);
    bp = header_to_payload(block);
    return bp;
} 

/*
 * free: Frees the block such that it is no longer allocated while still
 *       maintaining its size. Block will be available for use on malloc.
 */
void free(void *bp)
{
    bool prev, prev_16;
    if (bp == NULL)
    {
        return;
    }
    block_t *block = payload_to_header(bp);
    size_t size = get_size(block);

    prev = get_prev(block);
    prev_16 = get_16(block);
    write_header(block, size, false);
    write_footer(block, size, false);
    if (prev) set_prev(block);
    if (prev_16) set_16(block);

    block = coalesce(block);
    add_to_freelist(block);
}

/*
 * realloc: returns a pointer to an allocated region of at least size bytes:
 *          if ptrv is NULL, then call malloc(size);
 *          if size == 0, then call free(ptr) and returns NULL;
 *          else allocates new region of memory, copies old data to new memory,
 *          and then free old block. Returns old block if realloc fails or
 *          returns new pointer on success.
 */
void *realloc(void *ptr, size_t size)
{
    block_t *block = payload_to_header(ptr);
    size_t copysize;
    void *newptr;

    // If size == 0, then free block and return NULL
    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    // If ptr is NULL, then equivalent to malloc
    if (ptr == NULL)
    {
        return malloc(size);
    }

    // Otherwise, proceed with reallocation
    newptr = malloc(size);
    //dbg_printf("%d\n", (int)newptr);
    // If malloc fails, the original block is left untouched
    if (!newptr)
    {
        return NULL;
    }

    // Copy the old data
    copysize = get_payload_size(block); // gets size of old payload
    if(size < copysize)
    {
        copysize = size;
    }
    memcpy(newptr, ptr, copysize);

    // Free the old block
    free(ptr);

    return newptr;
}

/*
 * calloc: Allocates a block with size at least (elements * size + wsize)
 *         through malloc, then initializes all bits in allocated memory to 0.
 *         Returns NULL on failure.
 */
void *calloc(size_t nmemb, size_t size)
{
    void *bp;
    size_t asize = nmemb * size;

    if (asize/nmemb != size)
	// Multiplication overflowed
	return NULL;
    
    bp = malloc(asize);
    if (bp == NULL)
    {
        return NULL;
    }
    // Initialize all bits to 0
    memset(bp, 0, asize);

    return bp;
}

/******** The remaining content below are helper and debug routines ********/

/*
 * extend_heap: Extends the heap with the requested number of bytes, and
 *              recreates epilogue header. Returns a pointer to the result of
 *              coalescing the newly-created block with previous free block, if
 *              applicable, or NULL in failure.
 */
static block_t *extend_heap(size_t size) 
{
    void *bp;

    // Allocate an even number of words to maintain alignment
    size = round_up(size, dsize);
    if ((bp = mem_sbrk(size)) == (void *)-1)
    {
        return NULL;
    }
    
    // Initialize free block header/footer 
    block_t *block = payload_to_header(bp);
    write_header(block, size, false);
    write_footer(block, size, false);
    // Create new epilogue header
    block_t *block_next = find_next(block);
    write_header(block_next, 0, true);

    // Coalesce in case the previous block was free
    return coalesce(block);
}

/* Coalesce: Coalesces current block with previous and next blocks if
 *           either or both are unallocated; otherwise the block is not
 *           modified. Then, insert coalesced block into the segregated list.
 *           Returns pointer to the coalesced block. After coalescing, the
 *           immediate contiguous previous and next blocks must be allocated.
 */
static block_t *coalesce(block_t * block) 
{
    block_t *block_next = find_next(block);
    block_t *block_prev = NULL;
    bool prev_alloc = !(get_prev(block));
    bool next_alloc = get_alloc(block_next);
    size_t size = get_size(block);

    if (!prev_alloc) {
        block_prev = find_prev(block);
        prev_alloc = get_alloc(block_prev);
    }

    if (prev_alloc && next_alloc)              // Case 1
    {
        return block;
    }

    else if (prev_alloc && !next_alloc)        // Case 2
    {
        size += get_size(block_next);
        remove_from_freelist(block_next);
        write_header(block, size, false);
        write_footer(block, size, false);
    }

    else if (!prev_alloc && next_alloc)        // Case 3
    {
        size += get_size(block_prev);
        remove_from_freelist(block_prev);
        write_header(block_prev, size, false);
        write_footer(block_prev, size, false);
        block = block_prev;
    }

    else                                        // Case 4
    {
        size += get_size(block_next) + get_size(block_prev);
        remove_from_freelist(block_prev);
        remove_from_freelist(block_next);
        write_header(block_prev, size, false);
        write_footer(block_prev, size, false);
        block = block_prev;
    }
    return block;
}

/*
 * place: Places block with size of asize at the start of bp. If the remaining
 *        size is at least the minimum block size, then split the block to the
 *        the allocated block and the remaining block as free, which is then
 *        inserted into the segregated list. Requires that the block is
 *        initially unallocated.
 */
static void place(block_t *block, size_t asize)
{
    size_t csize = get_size(block);
    bool prev = get_prev(block);
    bool prev_16 = get_16(block);

    if (!just_extended)
        remove_from_freelist(block);
    else just_extended = false;


    if ((csize - asize) >= min_block_size)
    {
        block_t *block_next;
        write_header(block, asize, true);
        if (prev) set_prev(block);
        if (prev_16) set_16(block);

        block_next = find_next(block);
        write_header(block_next, csize-asize, true);
        free((void *)((block_next->payload)));
    }

    else
    { 
        write_header(block, csize, true);
        if (prev) set_prev(block);
    }
}

/*
 * find_fit: Looks for a free block with at least asize bytes with
 *           first-fit policy. Returns NULL if none is found.
 */
static block_t *find_fit(size_t asize)
{
    block_t *block, *free_listval;
    block_t **seg_lists = seg_lists_gb;
    size_t i, j;
    size_t start = seglist_addr_to_idx(find_list(asize));
    block_t *res = NULL;
    for (i = start; i < NUM_SEG_LISTS && res == NULL; i++)
    {
        free_listval = seg_lists[i];
        j = 0;
        for (block = free_listval; 
            block != NULL && j < 8; 
            block = block->d.next)
        {
            if (asize <= get_size(block)) {
                if (res == NULL || get_size(block) < get_size(res))
                {
                    res = block;
                    if (asize <= 512) j = 8;
                }
            j++;
            }
        }
    }
    return res;
}

/*
 * max: returns x if x > y, and y otherwise.
 */
static size_t max(size_t x, size_t y)
{
    return (x > y) ? x : y;
}


/*
 * round_up: Rounds size up to next multiple of n
 */
static size_t round_up(size_t size, size_t n)
{
    return (n * ((size + (n-1)) / n));
}

/*
 * pack: returns a header reflecting a specified size and its alloc status.
 *       If the block is allocated, the lowest bit is set to 1, and 0 otherwise
 */
static word_t pack(size_t size, bool alloc)
{
    return alloc ? (size | 1) : size;
}


/*
 * extract_size: returns the size of a given header value based on the header
 *               specification above.
 */
static size_t extract_size(word_t word)
{
    return (word & ~(word_t) 0xF);
}

/*
 * get_size: returns the size of a given block by clearing the lowest 4 bits
 *           (as the heap is 16-byte aligned).
 */
static size_t get_size(block_t *block)
{
    if (!get_alloc(block) && ((size_t)(block->header)&0x8))
        return 16;
    return extract_size(block->header);
}

/*
 * get_payload_size: returns the payload size of a given block, equal to
 *                   the entire block size minus the header size.
 */
static word_t get_payload_size(block_t *block)
{
    size_t asize = get_size(block);
    return asize - wsize;
}

/*
 * extract_alloc: returns the allocation status of a given header value based
 *                on the header specification above.
 */
static bool extract_alloc(word_t word)
{
    return (bool)(word & 0x1);
}

/*
 * get_alloc: returns true when the block is allocated based on the
 *            block header's lowest bit, and false otherwise.
 */
static bool get_alloc(block_t *block)
{
    dbg_requires(block != NULL);
    return extract_alloc(block->header);
}

/*
 * extract_prev: returns the prev status of a given header value based
 *                on the header specification above.
 */
static bool extract_prev(word_t word)
{
    return (bool)(word & 0x2);
}

/*
 * extract_16: returns the prev_16 status of a given header value based
 *                on the header specification above.
 */
static bool extract_16(word_t word)
{
    return (bool)(word & 0x4);
}

/*
 * get_prev: returns true when the block has free prev based on the
 *            block header's 2nd lowest bit, and false otherwise.
 */
static bool get_prev(block_t *block)
{
    dbg_requires(block != NULL);
    return extract_prev(block->header);
}

/*
 * get_16: returns true when the block has free prev of size 16, based on the
 *            block header's 3nd lowest bit, and false otherwise.
 */
static bool get_16(block_t *block)
{
    dbg_requires(block != NULL);
    return extract_16(block->header);
}

/*
 * set_prev: sets block's free_prev bit to true
 */
static void set_prev(block_t *block)
{
    dbg_requires(block != NULL);
    block->header = ((word_t)(block->header))|0x2;
}

/*
 * set_16: sets block's free_prev_16 bit to true
 */
static void set_16(block_t *block)
{
    dbg_requires(block != NULL);
    block->header = ((word_t)(block->header))|0x4;
}

/*
 * free_prev: sets block's free_prev bit to false
 */
static void free_prev(block_t *block)
{
    dbg_requires(block != NULL);
    block->header = ((word_t)(block->header))&(~((word_t)2));
}

/*
 * free_16: sets block's free_prev_16 bit to false
 */
static void free_16(block_t *block)
{
    dbg_requires(block != NULL);
    block->header = ((word_t)(block->header))&(~((word_t)4));
}

/*
 * write_header: given a block and its size and allocation status,
 *               writes an appropriate value to the block header.
 */
static void write_header(block_t *block, size_t size, bool alloc)
{
    block->header = pack(size, alloc);
}


/*
 * write_footer: given a block and its size and allocation status,
 *               writes an appropriate value to the block footer by first
 *               computing the position of the footer.
 */
static void write_footer(block_t *block, size_t size, bool alloc)
{
    word_t *footerp = (word_t *)((block->payload) + get_size(block) - dsize);
    *footerp = pack(size, alloc);
}


/*
 * find_next: returns the next consecutive block on the heap by adding the
 *            size of the block.
 */
static block_t *find_next(block_t *block)
{
    return (block_t *)(((char *)block) + get_size(block));
}

/*
 * find_prev_footer: returns the footer of the previous block.
 */
static word_t *find_prev_footer(block_t *block)
{
    dbg_requires(get_prev(block));
    // Compute previous footer position as one word before the header
    return (&(block->header)) - 1;
}

/*
 * find_prev: returns the previous block position by checking the previous
 *            block's footer and calculating the start of the previous block
 *            based on its size.
 */
static block_t *find_prev(block_t *block)
{
    dbg_requires(get_prev(block));
    if (get_16(block)) return (block_t *)((size_t)block-16);
    word_t *footerp = find_prev_footer(block);
    size_t size = extract_size(*footerp);
    return (block_t *)((char *)block - size);
}

/*
 * payload_to_header: given a payload pointer, returns a pointer to the
 *                    corresponding block.
 */
static block_t *payload_to_header(void *bp)
{
    return (block_t *)(((char *)bp) - offsetof(block_t, payload));
}

/*
 * header_to_payload: given a block pointer, returns a pointer to the
 *                    corresponding payload.
 */
static void *header_to_payload(block_t *block)
{
    return (void *)(block->payload);
}

// ensures that all blocks in free_list are free
bool no_alloc_in_freelist(block_t *free_listp)
{
    if (free_listp == NULL) return true;
    block_t *cur = free_listp;
    while (cur != NULL)
    {
        if (get_alloc(cur)) return false;
        cur = cur->d.next;
    }
    return true;
}

// ensures the the doubly linked free lists are well-connected
bool valid_nexts_and_prevs(block_t *free_listp, int idx)
{
    if (free_listp == NULL) return true;
    block_t *cur = free_listp;
    while (cur->d.next != NULL)
    {
        dbg_assert(cur->d.next->d.prev == cur);
        cur = cur->d.next;
    }
    return true;
}

//ensures that all free blocks are coalesced
bool all_coalesced(block_t *free_listp)
{
    if (free_listp == NULL) return true;
    block_t *cur = free_listp;
    while (cur->d.next != NULL)
    {
        if (get_prev(cur))
        {
            printf("%p\n", (void*)cur);
            dbg_assert(false);
        }
        cur = cur->d.next;
    }
    return true;
}

/* mm_checkheap: checks the heap for correctness; returns true if
 *               the heap is correct, and false otherwise.
 *               can call this function using mm_checkheap(__LINE__);
 *               to identify the line number of the call site.
 */
bool mm_checkheap(int lineno)  
{ 
    int i;
    size_t size;
    block_t* block;
    //heap_checks
    //epilogue check
    dbg_assert(get_size((block_t *)((word_t)mem_heap_hi()-7))==0);
    //prologue check
    dbg_assert(get_size((block_t *)((word_t)mem_heap_lo()+38*wsize))==0);
    for (block = heap_listp; get_size(block) > 0; block = find_next(block))
    {
        // alignment requirement
        dbg_assert((size_t)(block)%16 == 8);
        size = get_size(block);
        // no min block size test since min block size is 16 and we 
        // already checked that all pointers are 16 byte aligned.

        // same size in header and footer for all blocks with size > 16 (since 
        // they dont have footers)
        if (size > 16 && !get_alloc(block))
            dbg_assert(size == 
                extract_size(*((word_t *)((word_t)block+size-wsize))));
    }

    //free list checks
    for (i = 1; i < NUM_SEG_LISTS; i++)
    {
        //no loops in list (i.e. all blocks are unique)
        dbg_assert(no_loops(seg_lists_gb[i]));
        //no alloced block in free list
        dbg_assert(no_alloc_in_freelist(seg_lists_gb[i]));
        //all nexts and prevs are valid (well-formed doubly-linked list)
        dbg_assert(valid_nexts_and_prevs(seg_lists_gb[i],i));
        /* my implementation allows for some blocks to not coalesce so 
        * this it not an invariant. Making this an invariant would be a 
        * potential way to raise the util even further.
        */
    }
    (void)lineno; // placeholder so that the compiler
                  // will not warn about unused variable.
    return true;

}

/* add_to_freelist : adds a given block to seg_lists
*/
void add_to_freelist(block_t *block)
{  
    block_t **seg_lists = seg_lists_gb;
    size_t size = get_size(block);
    int idx;
    block_t **free_listptr = NULL;
    block_t *free_listval;

    if (size <= 16)
    {
        free_listval = seg_lists[0]; 
        block->d.next = free_listval;
        block->header = ((block->header)&0x7)|0x8;
        if (free_listval != NULL)
            free_listval->header = (free_listval->header&0x7) | (word_t)block;
        seg_lists[0] = block;
        set_prev(find_next(block));
        set_16(find_next(block));
        return;
    }

    free_listptr = find_list(size);
    free_listval = *free_listptr;
    idx = seglist_addr_to_idx(free_listptr);

    block->d.next = free_listval;
    block->d.prev = NULL;
    if (free_listval != NULL)
        (*free_listptr)->d.prev = block;
    *free_listptr = block;
    set_prev(find_next(block));
}



/* remove_from_freelist : removes a given block from seg_lists
*/
void remove_from_freelist(block_t *block)
{
    block_t **seg_lists = seg_lists_gb;
    size_t size = get_size(block);
    int idx;

    block_t **free_listptr = NULL;
    block_t *free_listval;

    //16 block-size handled separately due to unique case of prev in header
    if (size <= 16)
    {
        //case 1: 1-block list
        block_t *block_prev = (block_t *)(block->header & (~7));
        if (block->d.next == NULL && (int)block_prev == 8)
        {
           seg_lists[0] = NULL; 
        }
        //case 2 : block at end
        else if (block->d.next == NULL)
        {
            block_prev->d.next = NULL;
        }
        //case 2: block at beginning
        else if ((int)block_prev == 8)
        {
            block->d.next->header = (block->d.next->header&0xf) | 0x8;
            seg_lists[0] = block->d.next;
        }
        //else block is in middle:
        else {
            block->d.next->header = 
                (block->d.next->header&0x7) | (((word_t)block_prev)&(~0x7));
            block_prev->d.next = block->d.next;
        }
        block->header = (block->header & 0xf) | 16;
        free_prev(find_next(block));
        free_16(find_next(block)); 
        return; 
    }

    free_listptr = find_list(size);
    free_listval = *free_listptr;
    idx = seglist_addr_to_idx(free_listptr);
    //case 1: 1-block list
    if (block->d.next == NULL && block->d.prev == NULL)
    {
        *free_listptr = NULL; 
    }
    //case 2 : block at end
    else if (block->d.next == NULL)
    {
        block->d.prev->d.next = NULL;
    }
    //case 2: block at beginning
    else if (block->d.prev == NULL)
    {
        block->d.next->d.prev = NULL;
        *free_listptr = block->d.next;
    }
    //else block is in middle:
    else {
        block->d.next->d.prev = block->d.prev;
        block->d.prev->d.next = block->d.next;
    }
    free_prev(find_next(block));
}

/* no_loops : ensures all nodes and pointers in free_lists are unique or NULL
*/
bool no_loops(block_t *free_listp)
{
    block_t *tortoise = free_listp;
    if (!free_listp) return true;
    block_t *hare = free_listp->d.next;
    while (hare != NULL && tortoise != NULL)
    {
        if (hare == tortoise) return false;
        hare = hare->d.next;
        if (hare == NULL) return true;
        hare = hare->d.next;
        tortoise = tortoise->d.next;
    }
    return true;
} 

/* len_list : returns the length of a linked list
*/
size_t len_list(block_t* list)
{
    size_t res = 0;
    while (list != NULL)
    {
        res++;
        list = list->d.next;
    }
    return res;
}

/* in_lists : returns whether a block is in a list
 * Only for debugging.
 */
bool in_lists(block_t *block)
{
    block_t *cur;
    block_t **seg_lists = seg_lists_gb;
    int i;
    for (i = 0; i < NUM_SEG_LISTS; i++)
    {
        for (cur = seg_lists[i]; cur != NULL; cur = cur->d.next)
        {
            if (cur == block) return true;
        }
    }
    return false;
}

/* find_list : returns a pointer to the start of the correct seg_list for
 * a given size.
 */
block_t **find_list(size_t size)
{
    block_t **free_listptr;
    block_t **seg_lists = seg_lists_gb;
    if (size <= 512)
    {
        free_listptr = &seg_lists[(size/16)-1];
    }
    else if (size <= 1024)
    {
        free_listptr = &seg_lists[32];
    }
    else if (size <= 2048)
    {
        free_listptr = &seg_lists[33];
    }
    else if (size <= 4096)
    {
        free_listptr = &seg_lists[34];
    }
    else if (size <= 8192)
    {
        free_listptr = &seg_lists[35];
    }
    else
    {
        free_listptr = &seg_lists[36];
    }
    return free_listptr;
}

/* seglist_addr_to_idx : converts a seg_list addr to its index in
 * the array of seg_lists.
 */
size_t seglist_addr_to_idx(block_t **addr)
{
    block_t **seg_lists = seg_lists_gb;
    size_t res = (((uint64_t)addr - (uint64_t)&seg_lists[0])/8);
    return res;
}
