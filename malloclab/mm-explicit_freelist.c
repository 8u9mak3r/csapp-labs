/*
 *  =======================================================================================
 *  Solution 2.  Explicit List + LIFO
 *  =======================================================================================
*/


/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Smell Like 10 Spirit",
    /* First member's full name */
    "Dada",
    /* First member's email address */
    "hmdada1012@163.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* basic constants and macros */
#define WORDSIZE 4
#define DWORDSIZE 8
#define CHUNKSIZE (1 << 12)   // 4KB

/* pack size and allocation bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* read and write the content of a word */
#define READ(p) (*(unsigned int*)(p))
#define WRITE(p, val) (*(unsigned int*)(p) = (unsigned int)(val))

/* get the size and allocation status from a block */
#define GET_SIZE(p) ((READ(p)) & ~0x7)
#define GET_ALLOC(p) ((READ(p)) & 0x1)
#define GET_UNALLOCATABLE(p) ((READ(p)) & 0x2)

/* calculate the address of header and footer of a block */
#define HEADER_ADDR(p) ((char*)(p) - WORDSIZE)
#define FOOTER_ADDR(p) ((char*)(p) + GET_SIZE(HEADER_ADDR(p)) - DWORDSIZE)

/* calculate the address of previous block and next block*/
#define PREV_BLOCK_ADDR(p) ((char*)(p) - GET_SIZE(((char*)(p) - DWORDSIZE)))
#define NEXT_BLOCK_ADDR(p) ((char*)(p) + GET_SIZE(((char*)(p) - WORDSIZE)))

/* calculate the addr where the addr of the predecessor and the successor are stored */
#define GET_PRED(p) ((char*)(p))
#define GET_SUCC(p) (((char*)(p) + 4))


/* point to the first byte of the heap */
static void* heap_listp;
static void* free_firstp, *free_lastp;

/* new funcs */
static void* extend_heap(size_t);
static void* coalesce(void*);
static void* find_fit(size_t);
static void place(void*, unsigned int);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    heap_listp = free_firstp = free_lastp = NULL;

    if ((heap_listp = mem_sbrk(12 * WORDSIZE)) == (void*)-1) return -1;

    WRITE(heap_listp, 0);

    /* free_firstp */
    WRITE(heap_listp + WORDSIZE, PACK(2 * DWORDSIZE, 2));
    WRITE(heap_listp + (4 * WORDSIZE), PACK(2 * DWORDSIZE, 2));

    /* free_lastp */
    WRITE(heap_listp + (5 * WORDSIZE), PACK(2 * DWORDSIZE, 2));
    WRITE(heap_listp + (8 * WORDSIZE), PACK(2 * DWORDSIZE, 2));

    /* prologue block */
    WRITE(heap_listp + (9 * WORDSIZE), PACK(DWORDSIZE, 1));
    WRITE(heap_listp + (10 * WORDSIZE), PACK(DWORDSIZE, 1));

    /* epilogue block */
    WRITE(heap_listp + (11 * WORDSIZE), PACK(0, 1));

    free_firstp = heap_listp + DWORDSIZE;
    free_lastp = free_firstp + 2 * DWORDSIZE;
    heap_listp = free_lastp + 2 * DWORDSIZE;

    WRITE(GET_PRED(free_firstp), 0);
    WRITE(GET_SUCC(free_firstp), free_lastp);
    WRITE(GET_PRED(free_lastp), free_firstp);
    WRITE(GET_SUCC(free_lastp), 0);

    if (extend_heap(CHUNKSIZE) == (void*)-1) return -1;

    return 0;
}


/*
 * extend_heap - called when initializing or running out of memory
*/
static void* extend_heap(size_t size)
{
    size = (unsigned int)ALIGN(size);

    char* p = NULL;
    if ((p = mem_sbrk(size)) == (void*)-1) return (void*)-1;

    WRITE(HEADER_ADDR(p), PACK(size, 0));
    WRITE(FOOTER_ADDR(p), PACK(size, 0));
    WRITE(HEADER_ADDR(NEXT_BLOCK_ADDR(p)), PACK(0, 1));

    return coalesce(p);
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (size == 0) return NULL;

    // actual size required, including a footer, a header, and payload which is aligned to 8 bytes
    unsigned int newsize = ALIGN(size + DWORDSIZE);
   
    char* p = NULL;
    if ((p = find_fit(newsize)) != NULL)
    {
        place(p, newsize);
        return p;
    }

    unsigned int require = newsize < CHUNKSIZE ? CHUNKSIZE : newsize;
    if ((void*)(p = (char*)extend_heap(require)) == (void*)-1) return NULL;

    place(p, newsize);

    return p;
}


static void* find_fit(size_t size)
{
    /* first fit */
    void* p = (void*)READ(GET_SUCC(free_firstp));
    unsigned int is_unallocatable = GET_UNALLOCATABLE(HEADER_ADDR(p));

    while(!is_unallocatable)
    {
        size_t psize = GET_SIZE(HEADER_ADDR(p));
        unsigned int alloc = GET_ALLOC(HEADER_ADDR(p));

        if (!alloc && psize >= size) return p;
        p = (void*)READ(GET_SUCC(p));
        is_unallocatable = GET_UNALLOCATABLE(HEADER_ADDR(p));
    }
    
    return NULL;


    // /* best fit */
    // void *p = heap_listp, *bestp = NULL;
    // size_t M = 0x7fffffff;
    // while(GET_SIZE(HEADER_ADDR(p)) != 0)
    // {
    //     size_t p_size = GET_SIZE(HEADER_ADDR(p));
    //     unsigned int alloc = GET_ALLOC(HEADER_ADDR(p));
    //     if (!alloc && p_size >= size && p_size < M) bestp = p;

    //     p = NEXT_BLOCK_ADDR(p);
    // }

    // return bestp;
}


static void place(void* p, unsigned int size)
{
    unsigned int old_size = GET_SIZE(HEADER_ADDR(p));
    unsigned int old_alloc = GET_ALLOC(HEADER_ADDR(p));
    unsigned int remaining_size = old_size - size;
    assert((remaining_size & 0x7) == 0);

    if (remaining_size <= DWORDSIZE) size = old_size;    // minimum free block size is 16 bytes

    WRITE(HEADER_ADDR(p), PACK(size, 1));
    WRITE(FOOTER_ADDR(p), PACK(size, 1));

    /* in case that realloc calls place, if so, the block is already an allocated one */
    if (old_alloc)
    {
        if (remaining_size >= 2 * DWORDSIZE)
        {
            p = NEXT_BLOCK_ADDR(p);
            WRITE(HEADER_ADDR(p), PACK(remaining_size, 0));
            WRITE(FOOTER_ADDR(p), PACK(remaining_size, 0));

            void* succ = (void*)READ(GET_SUCC(free_firstp));
            WRITE(GET_PRED(p), free_firstp);
            WRITE(GET_SUCC(p), succ);
            WRITE(GET_SUCC(free_firstp), p);
            WRITE(GET_PRED(succ), p);
        }

        return;
    }

    void *pred = (void*)READ(GET_PRED(p)), *succ = (void*)READ(GET_SUCC(p));
    assert(pred != NULL && succ != NULL);

    if (remaining_size >= 2 * DWORDSIZE)    
    {
        /* replace with a new free block */
        p = NEXT_BLOCK_ADDR(p);
        WRITE(HEADER_ADDR(p), PACK(remaining_size, 0));
        WRITE(FOOTER_ADDR(p), PACK(remaining_size, 0));

        WRITE(GET_PRED(p), pred);
        WRITE(GET_SUCC(p), succ);
        
        WRITE(GET_SUCC(pred), p);
        WRITE(GET_PRED(succ), p);
    }
    else
    {
        /* remove a free block */
        WRITE(GET_SUCC(pred), succ);
        WRITE(GET_PRED(succ), pred);
    }
}


/*
 * mm_free - Freeing a block does nothing but removing header and footer
 */
void mm_free(void *ptr)
{
    coalesce(ptr);
}


/*
 * coalesce - merge adjacent free blocks
*/
static void* coalesce(void* ptr)
{
    unsigned int left_alloc = GET_ALLOC(HEADER_ADDR(PREV_BLOCK_ADDR(ptr)));
    unsigned int right_alloc = GET_ALLOC(HEADER_ADDR(NEXT_BLOCK_ADDR(ptr)));
    unsigned int left_size = GET_SIZE(HEADER_ADDR(PREV_BLOCK_ADDR(ptr)));
    unsigned int right_size = GET_SIZE(HEADER_ADDR(NEXT_BLOCK_ADDR(ptr)));
    unsigned int size = GET_SIZE(HEADER_ADDR(ptr));


    if (!left_alloc && !right_alloc)
    {
        void* next_ptr = NEXT_BLOCK_ADDR(ptr);

        /* merge with the left */
        size += (left_size + right_size);
        WRITE(HEADER_ADDR(PREV_BLOCK_ADDR(ptr)), PACK(size, 0));
        WRITE(FOOTER_ADDR(NEXT_BLOCK_ADDR(ptr)), PACK(size, 0));
        ptr = PREV_BLOCK_ADDR(ptr);

        /* remove the corresponding block from the free block list */
        void* pred = (void*)READ(GET_PRED(next_ptr));
        void* succ = (void*)READ(GET_SUCC(next_ptr));
        assert(pred != NULL && succ != NULL);

        WRITE(GET_SUCC(pred), succ);
        WRITE(GET_PRED(succ), pred);
    }
    else if (!left_alloc && right_alloc)
    {
        /* merge with the left */
        size += left_size;
        WRITE(HEADER_ADDR(PREV_BLOCK_ADDR(ptr)), PACK(size, 0));
        WRITE(FOOTER_ADDR(ptr), PACK(size, 0));
        ptr = PREV_BLOCK_ADDR(ptr);
    }
    else if (left_alloc && !right_alloc) 
    {
        /* replace with a new free block */
        void* next_ptr = NEXT_BLOCK_ADDR(ptr);
        void* pred = (void*)READ(GET_PRED(next_ptr));
        void* succ = (void*)READ(GET_SUCC(next_ptr));
        assert(pred != NULL && succ != NULL);

        WRITE(GET_PRED(ptr), pred);
        WRITE(GET_SUCC(ptr), succ);

        WRITE(GET_SUCC(pred), ptr);
        WRITE(GET_PRED(succ), ptr);

        size += right_size;
        WRITE(HEADER_ADDR(ptr), PACK(size, 0));
        WRITE(FOOTER_ADDR(ptr), PACK(size, 0));
    }
    else
    {
        /* push a new free block into the list */
        WRITE(HEADER_ADDR(ptr), PACK(size, 0));
        WRITE(FOOTER_ADDR(ptr), PACK(size, 0));

        void* succ = (void*)READ(GET_SUCC(free_firstp));
        assert(succ != NULL);

        WRITE(GET_PRED(ptr), free_firstp);
        WRITE(GET_SUCC(ptr), succ);

        WRITE(GET_SUCC(free_firstp), ptr);
        WRITE(GET_PRED(succ), ptr);
    }

    return ptr;
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) return mm_malloc(size);

    if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }


    size_t old_size = GET_SIZE(HEADER_ADDR(ptr));
    size_t new_size = ALIGN(size + DWORDSIZE);

    /* exploit adjacent free blocks */
    unsigned int left_alloc = GET_ALLOC(HEADER_ADDR(PREV_BLOCK_ADDR(ptr)));
    unsigned int right_alloc = GET_ALLOC(HEADER_ADDR(NEXT_BLOCK_ADDR(ptr)));
    unsigned int left_size = GET_SIZE(HEADER_ADDR(PREV_BLOCK_ADDR(ptr)));
    unsigned int right_size = GET_SIZE(HEADER_ADDR(NEXT_BLOCK_ADDR(ptr)));
    unsigned int current_size = GET_SIZE(HEADER_ADDR(ptr));

    void* lp = NULL, *rp = NULL, *pred = NULL, *succ = NULL;
    if (!left_alloc)
    {
        current_size += left_size;

        lp = PREV_BLOCK_ADDR(ptr);
        pred = (void*)READ(GET_PRED(lp));
        succ = (void*)READ(GET_SUCC(lp));
        assert(pred != NULL && succ != NULL);
        WRITE(GET_SUCC(pred), succ);
        WRITE(GET_PRED(succ), pred);
    }
    if (!right_alloc)
    {
        current_size += right_size;

        rp = NEXT_BLOCK_ADDR(ptr);
        pred = (void*)READ(GET_PRED(rp));
        succ = (void*)READ(GET_SUCC(rp));
        assert(pred != NULL && succ != NULL);
        WRITE(GET_SUCC(pred), succ);
        WRITE(GET_PRED(succ), pred);
    }

    if (lp != NULL)
    {
        memcpy(lp, ptr, old_size - DWORDSIZE);
        ptr = lp;
    }

    WRITE(HEADER_ADDR(ptr), PACK(current_size, 1));
    WRITE(FOOTER_ADDR(ptr), PACK(current_size, 1));


    if (new_size > current_size)    // allocate a new block and deallocate the old one
    {
        void* p = mm_malloc(size);
        if (p == NULL) return NULL;

        memcpy(p, ptr, old_size - DWORDSIZE);   // exclude header and footer
        mm_free(ptr);
        ptr = p;
    }
    else  // use the old one
    {
        place(ptr, new_size);
    }

    return ptr;
}

