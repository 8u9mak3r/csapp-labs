&emsp;&emsp;CS:APP Lab网站：<https://csapp.cs.cmu.edu/3e/labs.html>

&emsp;&emsp;点击`Self-Study Handout`和`writeup`下载到本地文件夹，使用命令`tar xvf malloc-handout.tar`将文件解压缩得到一个同名文件夹。文件夹里有很多的文件，参阅`writeup`，我们需要做的是修改`mm.c`中的四个函数：

- `int mm_init(void)`：堆初始化函数，成功返回0.出错返回-1。
- `void* mm_malloc(size_t size)`：语义和标准C库中的malloc函数一致，具体参阅writeup。
- `void mm_free(void* ptr)`：语义和标准C库中的malloc函数一致，具体参阅writeup。
- `void mm_realloc(void* ptr, size_t size)`：语义和标准C库中的realloc函数一致，具体参阅writeup。

&emsp;&emsp;在此之前先修改`makefile`文件，使之能根据命令行输入分别生成隐式空闲链表，显式空闲链表，和分离空闲链表。

&emsp;&emsp;文件夹里面两个`.rep`文件是测试用例，使用`make`命令编译链接得到测试驱动程序`mdriver`后，使用命令`./mdriver -f short1-bal.rep -V`和`./mdriver -f short2-bal.rep -V`就可以测试程序。这是两个小样例，更多的样例见[这里](https://github.com/Ethan-Yan27/CSAPP-Labs/tree/master/yzf-malloclab-handout/traces)。下载得到`traces`文件夹，放在和mdriver同级目录下，使用命令`/mdriver -t ./traces -V`就可以测试全部样例。我使用这个命令最后两个样例没有跑通，但是所有的样例单独跑却能跑通，没想明白为啥，干脆写了个shell脚本`run.sh`把每个样例单独跑一遍。

&emsp;&emsp;运行环境：Ubuntu 22.04虚拟机。

```shell
#! /bin/bash
cd ~/Desktop/cmu15213/malloclab/handout

./mdriver -f ./traces/hort1-bal.rep -a -V
./mdriver -f ./traces/short2-bal.rep -a -V
./mdriver -f ./traces/amptjp-bal.rep -a -V
./mdriver -f ./traces/binary-bal.rep -a -V
./mdriver -f ./traces/binary2-bal.rep -a -V
./mdriver -f ./traces/cccp-bal.rep -a -V
./mdriver -f ./traces/coalescing-bal.rep -a -V
./mdriver -f ./traces/cp-decl-bal.rep -a -V
./mdriver -f ./traces/expr-bal.rep -a -V
./mdriver -f ./traces/random-bal.rep -a -V
./mdriver -f ./traces/random2-bal.rep -a -V
./mdriver -f ./traces/realloc-bal.rep -a -V
./mdriver -f ./traces/realloc2-bal.rep -a -V
```

# 隐式空闲链表
&emsp;&emsp;书上基本把所有的代码实现都给出来了，除了`find_fit`，`place`和`mm_realloc`。除了之前说到的四个函数，其他函数作为内部函数对外部不可见，都声明成静态函数。

## find_fit
&emsp;&emsp;采用`first fit`方法，只要找到一块大小能够满足最近一次分配请求的块就立刻返回；若是采用`best_fit`方法，那么要遍历所有的块，从中找最小的能够满足分配请求的块。

```c
/* first-fit */
static void* first_fit(size_t size)
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
}


static void* best_fit(size_t size)
{
    // /* best fit */
    void *p = heap_listp, *bestp = NULL;
    size_t M = 0x7fffffff;
    while(GET_SIZE(HEADER_ADDR(p)) != 0)
    {
        size_t p_size = GET_SIZE(HEADER_ADDR(p));
        unsigned int alloc = GET_ALLOC(HEADER_ADDR(p));
        if (!alloc && p_size >= size && p_size < M) bestp = p;

        p = NEXT_BLOCK_ADDR(p);
    }

    return bestp;
}
```

## place
&emsp;&emsp;用`find_fit`函数找到的空闲块，满足当前分配请求后可能还有余，余的部分作为新的小空闲块。

&emsp;&emsp;2023.06.12更新：由于大小8字节的空闲块没有意义，无法存储payload，所以如果`remaining_size == 8`，直接分配出去，不作为单独的空闲块。
```c
static void place(void* p, unsigned int size)
{
    unsigned int old_size = GET_SIZE(HEADER_ADDR(p));
    unsigned int remaining_size = old_size - size;
    assert((remaining_size & 0x7) == 0);

    WRITE(HEADER_ADDR(p), PACK(size, 1));
    WRITE(FOOTER_ADDR(p), PACK(size, 1));

    if (remaining_size != 0) // set remaining part to be a new free block if any
    {
        WRITE(HEADER_ADDR(NEXT_BLOCK_ADDR(p)), PACK(remaining_size, 0));
        WRITE(FOOTER_ADDR(NEXT_BLOCK_ADDR(p)), PACK(remaining_size, 0));
    }
}
```

## realloc
&emsp;&emsp;这个函数的语义可以参考writeup或者C99 ISO标准手册，总结一下就是尽量使用自身以及周围的空闲块（如果有）的内存，迫不得已再使用`mm_malloc`重新分配。
&emsp;&emsp;别忘了两个分别等价于调用`mm_malloc`和`mm_free`的特殊情况。
```c
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
```

## 运行结果
&emsp;&emsp;命令行输入`make mdriver-implicit`，生成基于隐式空闲链表的mdriver测试程序。运行`run.sh`，每个样例的得分如下：
```shell
# 就列出测试文件名和得分，其他信息省略

Reading tracefile: short1-bal.rep
Perf index = 40 (util) + 40 (thru) = 80/100

Reading tracefile: short2-bal.rep
Perf index = 54 (util) + 40 (thru) = 94/100

Reading tracefile: ./traces/amptjp-bal.rep
Perf index = 60 (util) + 40 (thru) = 100/100

Reading tracefile: ./traces/binary-bal.rep
Perf index = 33 (util) + 6 (thru) = 39/100

Reading tracefile: ./traces/binary2-bal.rep
Perf index = 31 (util) + 5 (thru) = 36/100

Reading tracefile: ./traces/cccp-bal.rep
Perf index = 60 (util) + 40 (thru) = 100/100

Reading tracefile: ./traces/coalescing-bal.rep
Perf index = 40 (util) + 40 (thru) = 80/100

Reading tracefile: ./traces/cp-decl-bal.rep
Perf index = 60 (util) + 40 (thru) = 100/100

Reading tracefile: ./traces/expr-bal.rep
Perf index = 60 (util) + 40 (thru) = 100/100

Reading tracefile: ./traces/random-bal.rep
Perf index = 55 (util) + 40 (thru) = 95/100

Reading tracefile: ./traces/random2-bal.rep
Perf index = 55 (util) + 40 (thru) = 95/100

Reading tracefile: ./traces/realloc-bal.rep
Perf index = 26 (util) + 40 (thru) = 66/100

Reading tracefile: ./traces/realloc2-bal.rep
Perf index = 27 (util) + 40 (thru) = 67/100
```

## 一点补充
&emsp;&emsp;关于boundary tag，书上有这么一段：

> Fortunately, there is a clever optimization of boundary tags that eliminates the need for a footer in allocated blocks. Recall that when we attempt to coalesce the current block with the previous and next blocks in memory, the size field in the footer of the previous block is only needed if the previous block is free. If we were to store the allocated/free bit of the previous block in one of the excess low-order bits of the current block, then allocated blocks would not need footers, and we could use that extra space for payload. Note, however, that free blocks would still need footers.

&emsp;&emsp;仔细想了一下其实好像不太可行，为什么呢？假如现在有三块连续的已分配块，现在要释放中间一块。我们很容易能把最右边一块的header中表示上一块分配状态的位置0，因为定位右边一块只需要读取当前块的首部。但是没有上一块的footer，你不知道上一个块的大小，就无法在常数时间内定位到上一个块的header并修改对应的表示下一块分配状态的位。倒是可以一块一块一字节一字节地往前遍历读取，但是为了节省这点内存，你何必浪费这么多开销呢，万一这个块很大很大怎么办，反而得不偿失。

&emsp;&emsp;每一个块都有一个header和一个footer，虽然有点浪费内存，但是能维护块之间的隐式双向链表，这样向前向后找相邻块就很方便，诸如合并等一些操作在常数时间就可以完成。



# 显式空闲链表（LIFO实现）
&emsp;&emsp;因为程序不需要空闲块中的内容，所以可以把空闲块的主体部分利用起来，在原有隐式空闲表的基础上，把空闲块再另外组织成一个双向链表，每次分配的时候就从这个双向链表中找满足条件的空闲块。这样，可以把一次内存分配的复杂度从$O(totalblocks)$降至$O(freeblocks)$。

&emsp;&emsp;每个空闲块中，在本应该是有效载荷的部分包含有一个前驱指针`pred`和一个后继指针`succ`。两个指针值加上原来的两个`boundary tag`，使得32位系统下空闲块最少为16字节（Malloc Lab是32位程序，所以这里只说32位，64位是同理的）。

&emsp;&emsp;使用后进先出方式（LIFO）维护链表，每产生一个新的空闲块就把它放在链表头，每次分配的时候也总是首先从链表头开始遍历空闲块。结合之前的boundary tag，加上双向链表的插入删除操作，每次释放和合并操作依然是常数时间内完成。


## 基本宏定义
&emsp;&emsp;增加了获取空闲块的前驱后继指针的宏函数。
```c
/* get the size and allocation status from a block */
#define GET_SIZE(p) ((READ(p)) & ~0x7)
#define GET_ALLOC(p) ((READ(p)) & 0x1)
#define GET_UNALLOCATABLE(p) ((READ(p)) & 0x2)

/* calculate the addr where the addr of the predecessor and the successor are stored */
#define GET_PRED(p) ((char*)(p))
#define GET_SUCC(p) (((char*)(p) + 4))
```


## mm_init
&emsp;&emsp;除了之前的序言块`Prologue Block`，初始化时还要再加两个块，分别是空闲双向链表的头哨兵结点`free_firstp`和尾哨兵结点`free_lastp`。这两个块都是16字节，8字节的`boundary tag`和8字节的分别指向彼此的前驱后继指针，当然头哨兵的`pred`和尾哨兵的`succ`是置0的。

&emsp;&emsp;这里还有一个小技巧：初始的这三个块要把序言块放在最后，这样合并操作就不会误把那两个哨兵给合并了。哨兵节点`boundary tag`的最低位置0，第2位置1，表示空闲块但是不可被分配。

```c
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
```

## extend_heap
&emsp;&emsp;核心在于`coalesce`函数，之后会讲。
```c
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
```

## mm_malloc
&emsp;&emsp;没有变化。

## find_fit
&emsp;&emsp;不再需要遍历整个隐式链表了，遍历显式空闲链表就行了。
```c
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
}
```

## place
&emsp;&emsp;由于空闲块最小是16字节，使用当前空闲块满足内存分配请求后如果剩余空间小于等于8字节，那么这8字节也直接分配出去，不作保留。

&emsp;&emsp;这里有一个十分鸡贼的情况，就是一般malloc调用place的时候，空闲块p处于空闲状态。但若是realloc调用这个函数，块p事实上处于分配状态，前驱后继指针的存放位置存放的是未定义的内存位置。这里要加一个特判，不然程序会出现段错误。
```c
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

```

## mm_free
&emsp;&emsp;仅仅调用了`coalesce`函数。
```c
/*
 * mm_free - Freeing a block does nothing but removing header and footer
 */
void mm_free(void *ptr)
{
    coalesce(ptr);
}
```

## coalesce
&emsp;&emsp;在原来的基础上，根据前后内存块分配情况的不同，相应地对显式空闲链表进行节点合并，替换，插入，删除的操作。
```c
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
```

## mm_realloc
&emsp;&emsp;由于内存块ptr实际上处于已分配状态，前驱后继指针为未定义内存，所以在和前后空闲块合并时不能直接用coalesce函数，否则会崩溃，除此之外程序逻辑上和原来的mm_realloc没有太大区别。
```c
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
        int remaining = current_size - new_size;
        assert((remaining >= 0 && (remaining & 0x7) == 0));

        place(ptr, new_size);
    }

    return ptr;
}

```

## 运行结果
&emsp;&emsp;命令行输入`make mdriver-explicit`，生成基于显式空闲链表的mdriver测试程序。

&emsp;&emsp;两个binary测试集由原来的30多分变成了70多分，两个realloc测试集没啥变化，其他测试集都上到了90多分甚至接近100分。同时，对比于隐式空闲链表的实现，显式链表实现的吞吐率都达到了40分满分，执行效率大大提高。
```shell
Reading tracefile: short1-bal.rep
Perf index = 40 (util) + 40 (thru) = 80/100

Reading tracefile: short2-bal.rep
Perf index = 54 (util) + 40 (thru) = 94/100

Reading tracefile: ./traces/amptjp-bal.rep
Perf index = 56 (util) + 40 (thru) = 96/100

Reading tracefile: ./traces/binary-bal.rep
Perf index = 33 (util) + 40 (thru) = 73/100

Reading tracefile: ./traces/binary2-bal.rep
Perf index = 31 (util) + 40 (thru) = 71/100

Reading tracefile: ./traces/cccp-bal.rep
Perf index = 57 (util) + 40 (thru) = 97/100

Reading tracefile: ./traces/coalescing-bal.rep
Perf index = 40 (util) + 40 (thru) = 80/100

Reading tracefile: ./traces/cp-decl-bal.rep
Perf index = 58 (util) + 40 (thru) = 98/100

Reading tracefile: ./traces/expr-bal.rep
Perf index = 59 (util) + 40 (thru) = 99/100

Reading tracefile: ./traces/random-bal.rep
Perf index = 55 (util) + 40 (thru) = 95/100

Reading tracefile: ./traces/random2-bal.rep
Perf index = 53 (util) + 40 (thru) = 93/100

Reading tracefile: ./traces/realloc-bal.rep
Perf index = 26 (util) + 40 (thru) = 66/100

Reading tracefile: ./traces/realloc2-bal.rep
Perf index = 27 (util) + 40 (thru) = 67/100
```

