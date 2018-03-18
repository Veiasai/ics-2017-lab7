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

/*
516030910237 刘劲锋
显式双向链表结构
分配块：头（4字节）、分配、尾（4字节）
空闲块：头（4字节）、前向指针（8字节）、后向指针（8字节）、...、尾（4字节）
最小块24字节

优化：
1.realloc函数，传入指针为内存栈尾时，不需要memcopy。
2.每次从栈中申请新空间，有一个最小值，见define miniextend。
3.申请小块（小于miniextend），并且命中了空闲块链表时，记录cacheptr，下次小块分配不在该块（避免被等间距free的恶搞数据整死）。
*/

/* min extended memory*/
#define miniextend 480

 /* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define WSIZE 4
#define DSIZE 8
#define mini 24 //最小空闲块

#define PACK(size, alloc) ((size) | (alloc))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + (GET_SIZE((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - (GET_SIZE((char *)(bp) - DSIZE)))

void * find_fit(unsigned int asize);
void * place(void * bp, unsigned int asize);


//static int cnt_test = 0;

/* 空闲块链表获取 */
long unsigned int GET_FREE_PREV(void *p)
{
	return (*(long unsigned int *)(p));
}
long unsigned int GET_FREE_NEXT(void *p)
{
	return (*(long unsigned int *)(p+DSIZE));
}

/* 空闲块链表修改 */
void PUT_FREE_PREV(void * p, long unsigned int val)
{
	(*(long unsigned int *)((char *)(p)) = (val));
}

void PUT_FREE_NEXT(void * p, long unsigned int val)
{
	(*(long unsigned int *)(p+DSIZE)) = (val);
}

static void * cacheptr;
static void * free_ptr_head;
static void * alloc_ptr;
static void * alloc_end;
static void * alloc_head;

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	free_ptr_head = NULL;
	alloc_head = mem_sbrk(4);	// alignment 8
	PUT(alloc_head, PACK(8, 1));
	//fprintf(stderr,"headsbrk:%p\n", alloc_head);

	alloc_end = mem_sbrk(8) + 4;
	PUT(HDRP(alloc_end), PACK(8, 1));
	PUT(FTRP(alloc_end), PACK(8, 1));
	return 0;
}


/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(long unsigned int size)
{
	/*cnt_test++;
	fprintf(stderr, "cnt:%d\n", cnt_test);*/

	long unsigned int asize;
	long unsigned int extendsize;
	char * bp;
	if (size == 0)
		return NULL;

	if (size <= 16)
		asize = mini;
	else 
		asize = DSIZE * (((size + (DSIZE)+(DSIZE - 1)) / DSIZE));
	
	//fprintf(stderr,"size:%d, asize: %d\n", size, asize);
	
	if ((bp = find_fit(asize)) != NULL) {
		if (asize < miniextend)
			cacheptr = bp;
		else
			cacheptr = NULL;
		bp = place(bp, asize);
		return bp;
	}

	void * p;
	if ((asize <= miniextend) && ((miniextend - asize) >= mini))
	{
		p = mem_sbrk(miniextend);
		if (p == (void *)-1)
			return NULL;
		p = p - 4;	//结束块占8字节，前移4字节替代末尾块
		alloc_end += miniextend;
		PUT(HDRP(p), PACK(asize, 1));
		PUT(FTRP(p), PACK(asize, 1));
		PUT(HDRP(alloc_end), PACK(8, 1));
		PUT(FTRP(alloc_end), PACK(8, 1));
		
		unsigned int subsize = miniextend - asize;
		void * nextp = NEXT_BLKP(p);
		PUT(HDRP(nextp), PACK(subsize, 0));
		PUT(FTRP(nextp), PACK(subsize, 0));

		PUT_FREE_NEXT(nextp, free_ptr_head);	//添加至表头
		if (free_ptr_head != NULL)
			PUT_FREE_PREV(free_ptr_head, nextp);
		free_ptr_head = nextp;
		return p;
	}
	else {
		p = mem_sbrk(asize);
		if (p == (void *)-1)
			return NULL;
		else {
			p = p - 4;
			alloc_end += asize;
			PUT(HDRP(p), PACK(asize, 1));
			PUT(FTRP(p), PACK(asize, 1));
			PUT(HDRP(alloc_end), PACK(8, 1));
			PUT(FTRP(alloc_end), PACK(8, 1));
			return p;
		}
	}
}

void * place(void * bp, unsigned int asize)
{
	//fprintf(stderr, "%s\n", "call place");
	unsigned int test = GET_SIZE(HDRP(bp));
	//fprintf(stderr, "size:%x,\n", test);
	int size = (GET_SIZE(HDRP(bp))) - asize;
	
	if (size >= mini)   //大于最小块,空闲块继承链表位置，靠尾部的切为分配块。
	{
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
		void * nextp = NEXT_BLKP(bp);
		PUT(HDRP(nextp), PACK(asize, 1));
		PUT(FTRP(nextp), PACK(asize, 1));
		return nextp;
	}
	else    //无需分割出空闲块
	{
		PUT(HDRP(bp), PACK(test, 1));
		PUT(FTRP(bp), PACK(test, 1));
			if (bp == free_ptr_head) //被分配掉的空闲块是链表头，此时只需将头指向下一位置。
			{
				free_ptr_head = (void *)GET_FREE_NEXT(bp);
			}
			else    //空闲块非链表头，将前一node指向下一node。
			{
				void * prevptr = (void *)GET_FREE_PREV(bp);
				void * nextptr = (void *)GET_FREE_NEXT(bp);

				if (nextptr != NULL)	// 非链表尾，后一node指向前一node。
					PUT_FREE_PREV(nextptr, prevptr);
				PUT_FREE_NEXT(prevptr, nextptr);
			}
			return bp;
	}
}

void remove_my(void * bp)
{
	/*fprintf(stderr, "%s\n", "call remove");
	fprintf(stderr, "freehead:%p\n", free_ptr_head);
	fprintf(stderr, "re,bp:%p\n", bp);*/

	if (bp == free_ptr_head) //空闲块是链表头，此时只需将头指向下一位置。
	{
		//fprintf(stderr, "%s\n", "call re1");
		free_ptr_head = (void *)GET_FREE_NEXT(bp) ;
		//fprintf(stderr, "ptr:%p\n", free_ptr_head);
	}
	else    //空闲块非链表头，将前一node指向下一node。
	{
		//fprintf(stderr, "%s\n", "call re2");

		void * prevptr = (void *)GET_FREE_PREV(bp);
		void * nextptr = (void *)GET_FREE_NEXT(bp);

		//fprintf(stderr, "prevptr:%p\n", prevptr);
		//fprintf(stderr, "%s\n", "call re3");
		
		if (nextptr != NULL) // 非链表尾，后一node指向前一node。
			PUT_FREE_PREV(nextptr, prevptr);
		PUT_FREE_NEXT(prevptr, nextptr);

		//fprintf(stderr, "%s\n", "call re4");
	}
}

void * find_fit(unsigned int asize)
{
	/*fprintf(stderr, "%s\n", "call find");
	void * ptrtest = free_ptr_head;
	while (ptrtest != NULL)
	{
		fprintf(stderr, "%p\n", ptrtest);
		ptrtest = GET_FREE_NEXT(ptrtest);
	}*/

	void * ptr;
	ptr = free_ptr_head;
	while (ptr != NULL)
	{
		unsigned int size = GET_SIZE(HDRP(ptr));
		if (asize <= size && ptr != cacheptr)
		{
			//fprintf(stderr, "have found %p, asize:%d,\n", ptr, asize);
			return ptr;
		}
		ptr = GET_FREE_NEXT(ptr);
	}
	return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void * bp)
{
	/*cnt_test++;
	fprintf(stderr, "cnt:%d\n", cnt_test);
	fprintf(stderr, "%s\n", "call free");
	if ((GET_ALLOC(HDRP(bp)) != 1))
		return;*/
	
	unsigned int size = GET_SIZE(HDRP(bp));

	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp);
}

void coalesce(void * bp)
{
	/*fprintf(stderr, "%s\n", "call coalesce");
	void * ptr = free_ptr_head;
	while (ptr != NULL)
	{
		fprintf(stderr, "%p\n", ptr);
		ptr = GET_FREE_NEXT(ptr);
	}*/

	int prev_alloc = GET_ALLOC(bp - DSIZE);
	int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

	unsigned int size = (GET_SIZE(HDRP(bp)));
	//fprintf(stderr, "%s\n", "call co");
	//fprintf(stderr, "bp:%p\n", bp);
	if (prev_alloc && next_alloc)   //不需合并，空闲块插入到链表头
	{
		//fprintf(stderr, "%s\n", "call co1");
		if (free_ptr_head != NULL){
			PUT_FREE_PREV(free_ptr_head, bp);
		}
		PUT_FREE_NEXT(bp, free_ptr_head);
		free_ptr_head = bp;
	}
	else if (prev_alloc && !next_alloc) {   //合并后项，将链表有关内容复制,并修改前项指向和后项指向
		//fprintf(stderr, "%s:%p\n", "call co2", NEXT_BLKP(bp));
		void * nextp = NEXT_BLKP(bp);
		size = size + (GET_SIZE(HDRP(nextp)));
		void * nextp_pre = GET_FREE_PREV(nextp);
		void * nextp_nex = GET_FREE_NEXT(nextp);
		
		if (nextp_nex != NULL)
			PUT_FREE_PREV(nextp_nex, bp);
		if (nextp == free_ptr_head)
		{
			PUT_FREE_NEXT(bp, nextp_nex);
			free_ptr_head = bp;
		}
		else
		{
			PUT_FREE_NEXT(bp, nextp_nex);
			PUT_FREE_PREV(bp, nextp_pre);
			PUT_FREE_NEXT(nextp_pre, bp);
		}
		
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}

	else if (!prev_alloc && next_alloc) {    //合并前项，只需修改前项的大小
		//fprintf(stderr, "%s:%p\n", "call co3", PREV_BLKP(bp));
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	}

	else {      //前后同时合并，前项指针保留，后项指针消去，借用remove函数消去。
		//fprintf(stderr, "%s:%p\n", "call co4", PREV_BLKP(bp));
		size =size + (GET_SIZE(HDRP(PREV_BLKP(bp)))) + (GET_SIZE(FTRP(NEXT_BLKP(bp))));
		remove_my((void *)(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
	}
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	unsigned int has_size = GET_SIZE(HDRP(ptr));
	if ((ptr + has_size) == alloc_end)	// 需要重新分配的块正好在内存栈尾部
	{
		if (size == 0)	//alignment 8
			return NULL;

		if (size <= 16)
			size = mini;
		else
			size = DSIZE * (((size + (DSIZE)+(DSIZE - 1)) / DSIZE));

		//fprintf(stderr, "%s\n", "realloc 1");

		if (size > has_size)	//需要扩展
		{
			//fprintf(stderr, "%s\n", "realloc 2");
			mem_sbrk(size - has_size);
			alloc_end += size - has_size;	// 修改结束
			PUT(HDRP(alloc_end), PACK(8, 1));
			PUT(FTRP(alloc_end), PACK(8, 1));
			PUT(HDRP(ptr), PACK(size, 1));
			PUT(FTRP(ptr), PACK(size, 1));
		}
		//fprintf(stderr, "%s\n", "realloc 4");
		return ptr;
	}

	//正常复制
	void *oldptr = ptr;
	void *newptr;
	size_t copySize;

	newptr = mm_malloc(size);
	if (newptr == NULL)
		return NULL;
	copySize = has_size - DSIZE;
	if (size < copySize)
		copySize = size;
	memcpy(newptr, oldptr, copySize);
	mm_free(oldptr);
	return newptr;
}













