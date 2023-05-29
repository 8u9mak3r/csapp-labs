/*
 *  =======================================================================================
 *  Solution 1.  Implicit List
 *  =======================================================================================
*/


// /*
//  * mm-naive.c - The fastest, least memory-efficient malloc package.
//  * 
//  * In this naive approach, a block is allocated by simply incrementing
//  * the brk pointer.  A block is pure payload. There are no headers or
//  * footers.  Blocks are never coalesced or reused. Realloc is
//  * implemented directly using mm_malloc and mm_free.
//  *
//  * NOTE TO STUDENTS: Replace this header comment with your own header
//  * comment that gives a high level description of your solution.
//  */
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

/* calculate the address of header and footer of a block */
#define HEADER_ADDR(p) ((char*)(p) - WORDSIZE)
#define FOOTER_ADDR(p) ((char*)(p) + GET_SIZE(HEADER_ADDR(p)) - DWORDSIZE)

/* calculate the address of previous block and next block*/
#define PREV_BLOCK_ADDR(p) ((char*)(p) - GET_SIZE(((char*)(p) - DWORDSIZE)))
#define NEXT_BLOCK_ADDR(p) ((char*)(p) + GET_SIZE(((char*)(p) - WORDSIZE)))


/* point to the first byte of the heap */
static char* heap_listp;


/* new funcs */
static void* extend_heap(size_t);
static void* coalesce(void*);
static void* find_fit(size_t);
static void place(void*, unsigned int);
// static void printHeap(void);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // printf("CALL INIT\n");
    heap_listp = NULL;
    if ((heap_listp = mem_sbrk(4 * WORDSIZE)) == (void*)-1) return -1;

    WRITE(heap_listp, 0);
    WRITE(heap_listp + WORDSIZE, PACK(DWORDSIZE, 1));
    WRITE(heap_listp + (2 * WORDSIZE), PACK(DWORDSIZE, 1));
    WRITE(heap_listp + (3 * WORDSIZE), PACK(0, 1));
    heap_listp += 2 * WORDSIZE;

    if (extend_heap(CHUNKSIZE) == (void*)-1) return -1;
    // printHeap();
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
    // printHeap();
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
    unsigned int newsize = ALIGN(size + SIZE_T_SIZE);

    // printf("CALL MALLOC WITH %d\n", newsize);    
    char* p = NULL;
    if ((p = find_fit(newsize)) != NULL)
    {
        place(p, newsize);
        // printHeap();
        return p;
    }

    // printf("aaa\n");
    unsigned int require = newsize < CHUNKSIZE ? CHUNKSIZE : newsize;
    if ((void*)(p = (char*)extend_heap(require)) == (void*)-1) return NULL;

    // printHeap();
    place(p, newsize);

    // printHeap();
    return p;
}


static void* find_fit(size_t size)
{
    /* first fit */
    void* p = heap_listp;
    while (GET_SIZE(HEADER_ADDR(p)) != 0)  // epilogue block is characterized by size 0
    {
        size_t p_size = GET_SIZE(HEADER_ADDR(p));
        unsigned int alloc = GET_ALLOC(HEADER_ADDR(p));
        if (!alloc && p_size >= size) return p;

        p = NEXT_BLOCK_ADDR(p);
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
    unsigned int remaining_size = old_size - size;
    assert((remaining_size & 0x7) == 0);

    WRITE(HEADER_ADDR(p), PACK(size, 1));
    WRITE(FOOTER_ADDR(p), PACK(size, 1));

    if (remaining_size != 0)    // set remaining part to be a new free block if any
    {
        WRITE(HEADER_ADDR(NEXT_BLOCK_ADDR(p)), PACK(remaining_size, 0));
        WRITE(FOOTER_ADDR(NEXT_BLOCK_ADDR(p)), PACK(remaining_size, 0));
    }
}


/*
 * mm_free - Freeing a block does nothing but removing header and footer
 */
void mm_free(void *ptr)
{
    // printf("CALL FREE\n");
    unsigned int size = GET_SIZE(HEADER_ADDR(ptr));

    WRITE(HEADER_ADDR(ptr), PACK(size, 0));
    WRITE(FOOTER_ADDR(ptr), PACK(size, 0));

    coalesce(ptr);
    // printHeap();
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
        size += (left_size + right_size);
        WRITE(HEADER_ADDR(PREV_BLOCK_ADDR(ptr)), PACK(size, 0));
        WRITE(FOOTER_ADDR(NEXT_BLOCK_ADDR(ptr)), PACK(size, 0));
        ptr = PREV_BLOCK_ADDR(ptr);
    }
    else if (!left_alloc && right_alloc)
    {
        size += left_size;
        WRITE(HEADER_ADDR(PREV_BLOCK_ADDR(ptr)), PACK(size, 0));
        WRITE(FOOTER_ADDR(ptr), PACK(size, 0));
        ptr = PREV_BLOCK_ADDR(ptr);
    }
    else if (left_alloc && !right_alloc) 
    {
        size += right_size;
        WRITE(HEADER_ADDR(ptr), PACK(size, 0));
        WRITE(FOOTER_ADDR(ptr), PACK(size, 0));
    }

    return ptr;
}


// static void printHeap(void)
// {
//     char* p = heap_listp;
//     unsigned int size, alloc;

//     while((size = GET_SIZE(HEADER_ADDR(p))) != 0)
//     {
//         alloc = GET_ALLOC(HEADER_ADDR(p));
//         printf("%d %d\n", size, alloc);

//         p = NEXT_BLOCK_ADDR(p);
//     }

//     alloc = GET_ALLOC(HEADER_ADDR(p));
//     printf("%d %d\n\n\n", size, alloc);
// }


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

    size_t new_size = ALIGN(size + DWORDSIZE); // add header and footer, align to 8 bytes
    void* new_ptr = coalesce(ptr);  // merge with free blocks if any

    if (ptr != new_ptr)     // copy old content if previous block is free
    {
        memcpy(new_ptr, ptr, GET_SIZE(HEADER_ADDR(ptr)) - DWORDSIZE); // exclude header and footer
        ptr = new_ptr;
    }

    size_t current_size = GET_SIZE(HEADER_ADDR(ptr));
    assert((current_size & 0x7) == 0);

    // still allocated though having merged
    WRITE(HEADER_ADDR(ptr), PACK(current_size, 1));
    WRITE(FOOTER_ADDR(ptr), PACK(current_size, 1));

    
    if (new_size > current_size)    // allocate a new block and deallocate the old one
    {
        void* p = mm_malloc(new_size);
        if (p == NULL) return NULL;

        memcpy(p, ptr, current_size - DWORDSIZE);   // exclude header and footer
        mm_free(ptr);
        return p;
    }
    else if(new_size < current_size)  // new small free block emerges
    {
        int remaining = current_size - new_size;
        assert((remaining > 0 && (remaining & 0x7) == 0));

        place(ptr, new_size);
    }

    return ptr;
}