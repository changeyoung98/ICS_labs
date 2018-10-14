/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * 1. Use implicit free list to arrange the blocks. Each block has a header and a footer.
 * 2. Use first-fit to optimize utility. 
 * 3. Use realloc function that tries to utilize space near the current block.
 * 4. Optimize extend heap strategy.
 * 5. A check function that helps to debug
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"



/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8           /*Double word size*/
#define CHUNKSIZE (1<<12) /*the page size in bytes is 4K*/

#define MAX(x,y)    ((x)>(y)?(x):(y))

#define PACK(size,alloc)    ((size) | (alloc))

#define GET(p)  (*(unsigned int *)(p))
#define PUT(p,val)  (*(unsigned int *)(p) = (val))

#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)
#define GET_PRE(bp) (GET(FTRP(bp) - DSIZE))
#define GET_NEXT(bp) (GET(FTRP(bp) - WSIZE))
#define HDRP(bp)    ((char *)(bp)-WSIZE)
#define FTRP(bp)    ((char *)(bp)+GET_SIZE(HDRP(bp))-DSIZE)

#define NEXT_BLKP(bp)   ((char *)(bp)+GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))



static char *heap_listp = NULL;
static char *tail;
/*
 * mm_init - initialize the malloc package.
 * The return value should be -1 if there was a problem in performing the initialization, 0 otherwise
 */


static void *coalesce(void *bp){
    size_t  prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t  next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc) {
        return bp;
    }else if(prev_alloc && !next_alloc){
                size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
            PUT(HDRP(bp), PACK(size,0));
            PUT(FTRP(bp), PACK(size,0));
    }else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }else {
        size +=GET_SIZE(FTRP(NEXT_BLKP(bp)))+ GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words&0x1) ? (words+1)*WSIZE : words*WSIZE;
    if((long)(bp=mem_sbrk(size))==(void *)-1)
        return NULL;

    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));

    return coalesce(bp);
}
static void *find_fit(size_t size){
    void *bp;
    for(bp =tail; GET_SIZE(HDRP(bp))>0; bp = PREV_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))){
            return bp;
        }
    }
    return NULL;
}
static void place(void *bp,size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    if((csize-asize)>=(DSIZE+DSIZE)){
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize-asize,0));
        PUT(FTRP(bp),PACK(csize-asize,0));
    }else{
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
    }
}


int mm_init(void)
{
    if((heap_listp = mem_sbrk(4*WSIZE))==(void *)-1){
        return -1;
    }
    PUT(heap_listp,0);
    PUT(heap_listp+(1*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp+(2*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp+(3*WSIZE),PACK(0,1));
    heap_listp += (2*WSIZE);
    if(extend_heap(CHUNKSIZE/WSIZE)==NULL){
        return -1;
    }
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;
    if(size <=0) return NULL;

    if(size <= DSIZE){
        asize = DSIZE+DSIZE;
    }else{
        asize = (DSIZE)*((size+(DSIZE)+(DSIZE-1)) / (DSIZE));
    }
    if((bp = find_fit(asize))!= NULL){
        place(bp,asize);
        return bp;
    }
    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE))==NULL){
        return NULL;
    }
    place(bp,asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
/* $end mmfree */
    if(bp == NULL)
	return;

/* $begin mmfree */
    size_t size = GET_SIZE(HDRP(bp));
/* $end mmfree */

/* $begin mmfree */

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size ==0) {
	mm_free(ptr);
	return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
	return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
	return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}


int mm_check(void *bp)
{
show_allocated_block(bp);
show_HeapList();
  
}

void show_HeapList() {
	void * bp;
    if (heap_listp == NULL) {
        printf("NULL\n");
        return;
    }

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = GET_NEXT(bp)) {
        show_allocated_block(bp);
    }
    return;
}
void show_allocated_block(void * bp) {
    int size = GET_SIZE(HDRP(bp));
    int alloc = GET_ALLOC(HDRP(bp));
    printf("|Header:size=%d alloc=%d|...|pre=none|next=none|Footer|\n",
            size, alloc );
}
