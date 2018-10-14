/*
 * mm.c
 *
 * This is a malloc library using segregated fit.
 * Blocks are divided into three groups according to their size.
 * Blocks in each group are linked with explicit free list,
 * and is searched with first fit strategy.
 *
 * Free blocks will be coalesced when a block is freed.
 * However, the block created by extend heap will not be calesced
 * immediately.
 * 
 * To avoid copy in some situation, realloc has it own implementation.
 *
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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* Basic constants */
#define WSIZE 4 /* word size */
#define DSIZE 8 /* double word size */
#define MIN_BLK_SIZE 16 /* minimal block size */

/* max & min */
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* Pack & Unpack */
#define PACK(size, alloc) ((size) | (alloc))
#define SIZE(w) ((w) & ~0x7)
#define ALLOC(w) ((w) & 0x1)

/* Read & write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* compute address of header and footer from block pointer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + SIZE(GET(HDRP(bp))) - DSIZE) /* should set header correctly first */

/* compute address of next & previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + SIZE(GET((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - SIZE(GET((char *)(bp) - DSIZE)))

/* to move in the free list */
#define LEFT(bp) ((void *)GET(bp))
#define RIGHT(bp) ((void *)GET((char *)(bp) + WSIZE))
#define PUT_LEFT(bp, val) (*((unsigned int *)(bp)) = (unsigned int)(val))
#define PUT_RIGHT(bp, val) (*((unsigned int *)((char *)(bp) + WSIZE)) = (unsigned int)(val))

/* settings for each group, optimized for utilization */
#define G1_MAX 48
#define G2_MAX 432
#define CHUNKSIZE 432

/* for mm_check */
#define VISITED(bp) ((GET(HDRP(bp)) & 0x2) >> 1)
#define UNMARK(bp) PUT(HDRP(bp), GET(HDRP(bp)) - 0x2)
#define MARK(bp) PUT(HDRP(bp), GET(HDRP(bp)) + 0x2)

static void * heap_listp = NULL; /* pointer to block list's header */
static void * g1_header = NULL; /* pointer to group 1's header */
static void * g2_header = NULL; /* pointer to group 2's header */
static void * g3_header = NULL; /* pointer to group 3's header */

/* used in mm_check */
static int mm_list_check(void * header)
{
    void * bp = RIGHT(header);
    for (;bp != header; bp = RIGHT(bp)) {
        if (ALLOC(GET(HDRP(bp)))) {
            printf("block %p is not free but is in free list\n", bp);
            return 0; /* invariant 1 check failed */
        }
        if (VISITED(bp)) {
            printf("block %p is in more than one free list\n", bp);
            return 0; /* invariant 2 check failed */
        } else {
            MARK(bp); /* mark as visited */
        }
    }
    return 1;
}

/** 
 * mm_check
 *
 * Check for invariants below:
 * 1. for all free list, every block in it must be marked as free.
 * 2. for all free block, it must be in exactly one free list.
 * 3. the information in header and footer of same block must 
 * consistent.
 *
 * Note: In our strategy, contiguous free blocks are allowed.
 */
int mm_check()
{
    int res = 0;
    void * bp = NULL;

    /* check for each free list */
    res = mm_list_check(g1_header);
    if (res == 0) {
        return res;
    }
    res = mm_list_check(g2_header);
    if (res == 0) {
        return res;
    }
    res = mm_list_check(g3_header);
    if (res == 0) {
        return res;
    }

    for (bp = heap_listp; SIZE(GET(HDRP(bp))) > 0; bp = NEXT_BLKP(bp)) {
        if (!ALLOC(GET(HDRP(bp)))) {
            if (VISITED(bp)) {
                UNMARK(bp); /* remove visited flag */
            } else {
                printf("block %p is not in any free list\n", bp);
                return 0; /* invariant 2 check failed */
            }
        }
        if (GET(HDRP(bp)) != GET(FTRP(bp))) {
            printf("information inconsistent at %p, header{size:%d, alloc:%d}, footer{size:%d, alloc:%d}\n",
                    bp, SIZE(GET(HDRP(bp))), ALLOC(GET(HDRP(bp))), SIZE(GET(FTRP(bp))), ALLOC(GET(FTRP(bp))));
            return 0; /* invariant 3 check failed */

        }
    }
    return 1;
}

/* insert a free block into the free list it belongs to */
static void insert_free_block(void * bp)
{
    void * header = NULL;
    size_t bsize = SIZE(GET(HDRP(bp)));

    /* decide the group it belongs to */
    if (bsize <= G1_MAX) {
        header = g1_header;
    } else if (bsize <= G2_MAX) {
        header = g2_header;
    } else {
        header = g3_header;
    }

    /* insert at the header of the list */
    PUT_LEFT(bp, header);
    PUT_RIGHT(bp, RIGHT(header));
    PUT_LEFT(RIGHT(header), bp);
    PUT_RIGHT(header, bp);
}

/* remove block from the free list */
static void remove_free_block(void * bp)
{
    PUT_RIGHT(LEFT(bp), RIGHT(bp));
    PUT_LEFT(RIGHT(bp), LEFT(bp));
}

/* merge free blocks */
static void * coalesce(void * bp)
{
    size_t prev_alloc = ALLOC(GET(FTRP(PREV_BLKP(bp))));
    size_t next_alloc = ALLOC(GET(HDRP(NEXT_BLKP(bp))));
    size_t size = SIZE(GET(HDRP(bp)));

    if (!prev_alloc && !next_alloc) { /* case 1 */
        remove_free_block(PREV_BLKP(bp));
        remove_free_block(NEXT_BLKP(bp));

        size += SIZE(GET(HDRP(PREV_BLKP(bp)))) + SIZE(GET(FTRP(NEXT_BLKP(bp))));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else if (!prev_alloc && next_alloc) { /* case 2 */
        remove_free_block(PREV_BLKP(bp));
        size += GET(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        PUT(FTRP(bp), PACK(size, 0));
    } else if (prev_alloc && !next_alloc) { /* case 3 */
        remove_free_block(NEXT_BLKP(bp));
        size += SIZE(GET(HDRP(NEXT_BLKP(bp))));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    /* in case 4, the previous and next blocks are allocated,
     * can not coalesce */

    insert_free_block(bp);
    return bp;
}

/* place a request into a block, may split the block */
static void place(void * bp, size_t asize)
{
    size_t bsize = SIZE(GET(HDRP(bp)));
    remove_free_block(bp);

    /* suitable for split? */
    if ((bsize - asize) >= MIN_BLK_SIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(bsize - asize, 0));
        PUT(FTRP(bp), PACK(bsize - asize, 0));
        insert_free_block(bp);
    } else {
        PUT(HDRP(bp), PACK(bsize, 1));
        PUT(FTRP(bp), PACK(bsize, 1));
    }
}

/* extend the heap, the new block is not coalesced nor added to free list */
static void * extend_heap(size_t bytes)
{
    size_t size = ALIGN(bytes);
    char * bp = mem_sbrk(size);
    if (bp == (char *)-1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0)); /* free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    return bp;
}

/* using first fit to find suitable block in each group
 * if not found, will extend the heap */
static void * find_fit_group(void * header, size_t asize, size_t esize)
{
    void * bp = RIGHT(header);
    for (;bp != header; bp = RIGHT(bp)) {
        if (asize <= SIZE(GET(HDRP(bp)))) {
            return bp;
        }
    }

    bp = extend_heap(MAX(asize, esize));
    insert_free_block(bp);
    return bp;
}

/* using segregated fit */
static void * find_fit(size_t asize)
{
    if (asize <= G1_MAX) {
        return find_fit_group(g1_header, asize, G1_MAX);
    } else if (asize <= G2_MAX) {
        return find_fit_group(g2_header, asize, G2_MAX);
    } else {
        return find_fit_group(g3_header, asize, CHUNKSIZE);
    }
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    void * p = NULL;
    /* create the minimal heap first */
    heap_listp = mem_sbrk(10 * WSIZE);
    if (heap_listp == (void *)-1) {
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(WSIZE * 8, 1)); /* Prologue header */
    PUT(heap_listp + (8 * WSIZE), PACK(WSIZE * 8, 1)); /* Prologue header */
    PUT(heap_listp + (9 * WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_listp += DSIZE;

    /* set list header */
    g1_header = heap_listp;
    PUT_LEFT(g1_header, g1_header);
    PUT_RIGHT(g1_header, g1_header);

    g2_header = heap_listp + DSIZE;
    PUT_LEFT(g2_header, g2_header);
    PUT_RIGHT(g2_header, g2_header);

    g3_header = heap_listp + DSIZE * 2;
    PUT_LEFT(g3_header, g3_header);
    PUT_RIGHT(g3_header, g3_header);

    /* extend the heap to create the inital free block */
    p = extend_heap(G1_MAX);
    insert_free_block(p);
    
    return 0;
}

/* 
 * mm_malloc - allocates a block
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    char * bp = NULL;

    if (size == 0) {
        return NULL;
    }

    /* calculate size for allocation */
    asize = ALIGN(size + DSIZE);

    bp = find_fit(asize);
    if (bp != NULL) {
        place(bp, asize);
        return bp;
    }

    return NULL;
}

/*
 * mm_free - Freeing a block.
 */
void mm_free(void *bp)
{
    size_t size = SIZE(GET(HDRP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - only moves block when there is no other way
 */
void *mm_realloc(void *bp, size_t size)
{
    void *p;
    size_t bsize;
    size_t asize = ALIGN(size + DSIZE);

    if (bp == NULL) {
        return mm_malloc(size);
    }

    bsize = SIZE(GET(HDRP(bp)));

    if (size == 0) {
        mm_free(bp);
        return NULL;
    } else if (asize <= bsize) { /* shrinking block? */
        if ((bsize - asize) >= MIN_BLK_SIZE) { /* create a new free block? */
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));
            p = NEXT_BLKP(bp);
            PUT(HDRP(p), PACK(bsize - asize, 0));
            PUT(FTRP(p), PACK(bsize - asize, 0));
            coalesce(p);
        }
    } else { /* extend allocated block */
        if (SIZE(GET(HDRP(NEXT_BLKP(bp)))) == 0) { /* is last block - just extend heap */
            extend_heap(asize - bsize);
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));
        } else if (!ALLOC(GET(HDRP(NEXT_BLKP(bp)))) && (bsize + SIZE(GET(HDRP(NEXT_BLKP(bp)))) >= asize)) { /* can merge next block? */
            remove_free_block(NEXT_BLKP(bp));
            bsize += SIZE(GET(HDRP(NEXT_BLKP(bp))));
            /* placing it */
            if ((bsize - asize) >= MIN_BLK_SIZE) {
                PUT(HDRP(bp), PACK(asize, 1));
                PUT(FTRP(bp), PACK(asize, 1));
                p = NEXT_BLKP(bp);
                PUT(HDRP(p), PACK(bsize - asize, 0));
                PUT(FTRP(p), PACK(bsize - asize, 0));
                coalesce(p);
            } else {
                PUT(HDRP(bp), PACK(bsize, 1));
                PUT(FTRP(bp), PACK(bsize, 1));
            }
        } else { /* block have to be moved */
            p = mm_malloc(size);
            if (p == NULL)
              return NULL;
            memcpy(p, bp, MIN(size, bsize));
            mm_free(bp);
            bp = p;
        }
    }
    return bp;
}

