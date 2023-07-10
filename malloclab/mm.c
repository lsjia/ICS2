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
    "ateam",
    /* First member's full name */
    "Luo Sijia",
    /* First member's email address */
    "2021201679@ruc.edu.cn",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

#define VERBOSE 0
#ifdef DEBUG
#define VERBOSE 1
#endif

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define WSIZE 4             // 字
#define DSIZE 8             // 双字
#define CHUNKSIZE (1 << 12) // 每次增大堆的大小

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))
/* Read and write a word at address p */
#define GET(p) (*(int *)(p))
#define PUT(p, val) (*(int *)(p) = (val))
/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

static void *heap_listp; /* pointer to first block */

static void *extend_heap(size_t size);     // 扩大堆
static void place(void *bp, size_t asize); // 放置并分割
static void *find_fit(size_t asize);       // 寻找空闲块
static void *coalesceFreeBlock(void *bp);  // 合并

static void *next_fitptr;                           // 下一次匹配指向的指针
static int mm_check(int verbose, const char *func); // 检查

static void *extend_heap(size_t size)
{
    size_t asize;
    void *bp;

    asize = ALIGN(size);
    if ((long)(bp = mem_sbrk(asize)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));         // 空闲块的头部
    PUT(FTRP(bp), PACK(size, 0));         // 空闲块的脚部
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 新的结尾块

    return coalesceFreeBlock(bp);
}

static void place(void *bp, size_t asize) // 分配并分割
{
    size_t old_size = GET_SIZE(HDRP(bp));   // 原来的大小
    size_t unalloc_size = old_size - asize; // 分配后剩余的大小

    if (unalloc_size < 2 * DSIZE) // 如果剩余的块小于最小块，不分割
    {
        PUT(HDRP(bp), PACK(old_size, 1));
        PUT(FTRP(bp), PACK(old_size, 1));
    }
    else // 剩余的块大于最小块，分割
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp); // 指到剩余的空闲块
        PUT(HDRP(bp), PACK(unalloc_size, 0));
        PUT(FTRP(bp), PACK(unalloc_size, 0));
    }
}

static void *find_fit(size_t asize) // next fit
{
    char *hi_ptr = (char *)mem_heap_hi() + 1;
    next_fitptr = NEXT_BLKP(next_fitptr); // 此次开始搜索的位置
    void *start_ptr = next_fitptr;

    while (next_fitptr != hi_ptr) // 找start_ptr和hi_ptr之间的空闲块
    {
        if (!GET_ALLOC(HDRP(next_fitptr)) && (GET_SIZE(HDRP(next_fitptr)) >= asize))
            return next_fitptr;
        next_fitptr = NEXT_BLKP(next_fitptr);
    }
    // 如果没找到，就从头开始找
    next_fitptr = NEXT_BLKP(heap_listp);
    while (next_fitptr != start_ptr)
    {
        if (!GET_ALLOC(HDRP(next_fitptr)) && (GET_SIZE(HDRP(next_fitptr)) >= asize))
            return next_fitptr;
        next_fitptr = NEXT_BLKP(next_fitptr);
    }

    return NULL;

    /*first fit
    void *ptr;
    for(ptr = heap_listp; GET_SIZE(HDRP(ptr))>0; ptr = NEXT_BLKP(ptr))
    {
        if(!GET_ALLOC(HDRP(ptr)) && (GET_SIZE(HDRP(ptr)) >= asize))
            return ptr;
    }
    return NULL;*/
}

static void *coalesceFreeBlock(void *bp) // 合并空闲块
{
    int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    int prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (!prev_alloc) // 前一个是空闲的
    {
        if (bp == next_fitptr) // 如果next_fitptr与bp相同，也要更新next_ptr，防止被合并掉
        {
            next_fitptr = PREV_BLKP(bp);
        }
        bp = PREV_BLKP(bp);
        size += GET_SIZE(HDRP(bp));

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    if (!next_alloc) // 后一个是空闲的
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    return bp;
}
/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) // 分配失败
    {
        printf("ERROR: mem_sbrk failed in mm_init\n");
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1)); // prologue header
    PUT(heap_listp + DSIZE, PACK(DSIZE, 1)); // prologue footer
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1)); // epilogue header

    heap_listp += DSIZE;
    next_fitptr = heap_listp; // 初始化
    if (extend_heap(CHUNKSIZE) == NULL)
        return -1;

    // if(mm_check(VERBOSE, __func__) == 0)
    //     printf("==================================\n");
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t addSize;
    char *bp = NULL;

    if (size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);

        // if(mm_check(VERBOSE, __func__) == 0)
        //     printf("==================================\n");
        return bp;
    }

    // 没找到合适的块
    addSize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(addSize)) == NULL)
        return NULL;
    place(bp, asize);

    // if(mm_check(VERBOSE, __func__) == 0)
    //     printf("==================================\n");
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesceFreeBlock(ptr); // 合并

    // if(mm_check(VERBOSE, __func__) == 0)
    //     printf("==================================\n");
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
    {
        return mm_malloc(size);
    }
    if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }

    size_t newSize = ALIGN(size + 2 * WSIZE); // 对齐
    size_t oldSize = GET_SIZE(HDRP(ptr));
    void *new_ptr = NULL;
    if (newSize > oldSize)
    {
        new_ptr = find_fit(newSize); // 寻找适合的空闲块
        if (!new_ptr)                // 没有合适的空闲块
        {
            size_t addSize = MAX(newSize, CHUNKSIZE);
            new_ptr = extend_heap(addSize);
            if (new_ptr == NULL)
                return NULL;
        }
        place(new_ptr, newSize);
        memcpy(new_ptr, ptr, oldSize - 2 * WSIZE); // 拷贝过去
        mm_free(ptr);                              // 释放原来的块

        // if(mm_check(VERBOSE, __func__) == 0)
        //     printf("==================================\n");
        return new_ptr;
    }
    else // 将之前的size缩小
    {
        if (oldSize - newSize < 2 * DSIZE) // 小于最小块，不分割
        {
            return ptr;
        }
        else
        {
            place(ptr, newSize);

            // if(mm_check(VERBOSE, __func__) == 0)
            //     printf("==================================\n");
            return ptr;
        }
    }

    return NULL;
}

static int mm_check(int verbose, const char *func)
{
    if (!verbose)
        return 1;
    void *p;
    for (p = heap_listp; GET_SIZE(HDRP(p)) > 0; p = NEXT_BLKP(p))
    {
        if (GET_ALLOC(HDRP(p)) == 0 && GET_ALLOC(HDRP(NEXT_BLKP(p))) == 0) // 空闲块未合并
        {
            printf("ERROR:%p and the next free block are not coalesced.\n", p);
            printf("hsize = %d, fsize = %d\n", GET_SIZE(HDRP(p)), GET_SIZE(FTRP(p)));
            printf("halloc = %d, falloc = %d\n", GET_ALLOC(HDRP(p)), GET_ALLOC(FTRP(p)));
            printf("next_head_alloc = %d, next_footer_alloc = %d\n", GET_ALLOC(HDRP(NEXT_BLKP(p))), GET_ALLOC(FTRP(NEXT_BLKP(p))));
            return 0;
        }
        if (GET(HDRP(p)) != GET(FTRP(p))) // 头部和脚部不对应
        {
            printf("ERROR: %p's header and footer are not matched.\n", p);
            printf("hsize = %d, fsize = %d\n", GET_SIZE(HDRP(p)), GET_SIZE(FTRP(p)));
            printf("halloc = %d, falloc = %d\n", GET_ALLOC(HDRP(p)), GET_ALLOC(FTRP(p)));
            return 0;
        }
        if ((int)p % ALIGNMENT != 0) // payload没对齐
        {
            printf("ERROR: %p's Payload area is not aligned.\n", p);
            return 0;
        }
        if (GET_SIZE(HDRP(p)) % ALIGNMENT != 0) // 大小没对齐
        {
            printf("ERROR: %p payload size is not doubleword aligned.\n", p);
            return 0;
        }
    }
    return 1;
}
