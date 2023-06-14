# 实验准备
&emsp;&emsp;CS:APP Lab网站：<https://csapp.cs.cmu.edu/3e/labs.html>，实验用到的材料都可以在官网找到，参阅writeup就可以知道如何操作。

## 环境配置
&emsp;&emsp;本人实验环境配置：
- Intel Core i7处理器（x86-64）
- Ubuntu-22.04 虚拟机
- gcc-11.3.0
- valgrind-3.18.1
  
&emsp;&emsp;终端键入`valgrind --version`命令看看有没有安装valgrind，若没有，键入命令`sudo apt install valgrind进行安装`。

## 参考跟踪文件
&emsp;&emsp;`traces`文件夹下的参考跟踪文件用于测试Part A中我们编写的Cache模拟器是否正确工作。这些参考跟踪文件都是由`valgrind`跟踪工具生成的。例如，键入命令：
```shell
linux> valgrind --log-fd=1 --tool=lackey -v --trace-mem=yes ls -l
```

&emsp;&emsp;运行程序`ls -l`，同时valgrind工具会在程序执行过程中按顺序捕获每一次对内存的访问并打印在终端上。

&emsp;&emsp;valgrind访存跟踪有如下格式
```
I 0400d7d4,8
 M 0421c7f0,4
 L 04f6b868,8
 S 7ff0005c8,8
```

&emsp;&emsp;每行表示1-2个内存访问，每行的格式是`[space]operation address,size`。`operation`字段表示内存访问的类型：`I`表示指令加载，`L`表示数据加载，`S`表示数据存储，`M`表示数据修改（即数据加载后跟数据存储，这个操作有两次访存）。每个`I`之前都没有空格，其他之前总是有一个空格。`address`字段指定64位十六进制内存地址。`size`字段指定操作访问的字节数。

&emsp;&emsp;由于x86体系结构中指令长短不一且不对齐，加上指令Cache和数据Cache是分开的，这个实验中我们更关注对数据Cache的模拟。所以过程中忽略所有的取指令操作`I`。同时我们假设所有的访存地址都是合理对齐的，不会出现所要访问的数据分别出现在两个缓存块的情况。比如，假设缓存块大小16字节，样例中是不会出现访问内存0xa~0x1a的16字节的数据的操作的，**但是取指令操作说不定会出现，因为指令在内存里是不对齐的**。这样一来实现会简单很多，我们不再需要关心访问的字节数，只需定位到相应的缓存块即可。


# Part A. Cache Simulator
&emsp;&emsp;这部分实验要求实现一个Cache模拟器，其能接受的命令行选项与样例运行结果需要与参考模拟器`csim-ref`一样。

## 命令行解析
&emsp;&emsp;终端键入`./csim-ref -h`，其接受的命令行选项如下：
```shell
Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>
Options:
  -h         Print this help message.
  -v         Optional verbose flag.
  -s <num>   Number of set index bits.
  -E <num>   Number of lines per set.
  -b <num>   Number of block offset bits.
  -t <file>  Trace file.

Examples:
  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace
  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace
```

&emsp;&emsp;命令行样例在writeup中也给了。命令行键入`man 3 getopt`可以在终端查看如何使用`getopt.h`头文件中的函数和全局变量来解析命令行，这里不多介绍。命令行解析函数如下：
```c
/* parse optional arguments in command */
static void parse_args(int argc, char* argv[], char* optstring)
{
    int opt;
    while((opt = getopt(argc, argv, optstring)) != -1)
    {
        switch (opt)
        {
            case 'h':
                print_help();
                exit(0);

            case 'v':
                verbose_flag = 1;
                break;

            case 's':
                s = atoi(optarg);
                S = 1 << s;
                break;

            case 'E':
                E = atoi(optarg);
                break;

            case 'b':
                b = atoi(optarg);
                B = 1 << b;
                break;

            case 't':
                strcpy(tracefile, optarg);
                break;
        
            default:
                print_help();
                exit(0);
                break;
        }
    }
}


/* called when the option -h or an undefined option is parsed */
static void print_help(void)
{
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n\n");
    printf("Examples:\n");
    printf("  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}
```

## 缓存初始化
&emsp;&emsp;整个实验只是模拟缓存的命中、不命中与牺牲块的选择，用不到缓存中保存的数据实体。所以只需要初始化三个大小$S \times E$的矩阵：块标记`tags`、有效位`valid_bits`、和时间戳`timestamp`。

```c
static void init_cache(void)
{
    tags = malloc(S * sizeof(unsigned long long*));
    for (int i = 0; i < S; i++) tags[i] = malloc(E * sizeof(unsigned long long));

    valid_bits = malloc(S * sizeof(int*));
    for (int i = 0; i < S; i++) valid_bits[i] = malloc(E * sizeof(int));

    timestamp = malloc(S * sizeof(int*));
    for (int i = 0; i < S; i++) timestamp[i] = malloc(E * sizeof(int));

    for (int i = 0; i < S; i++)
    {
        for (int j = 0; j < E; j++)
        {
            valid_bits[i][j] = 0;
            timestamp[i][j] = 0x7fffffff;
        }
    }
}
```

## LRU策略
&emsp;&emsp;时间戳`timestamp`用于实现LRU替换算法，它的值可以理解为用于表示一个块距离现在有多久没有被访问过了。当$E \geq 2$时，出现了未命中需要选择一个牺牲块的时候，总是选择时间戳值最大的块去牺牲。

&emsp;&emsp;本实验中采取的LRU实现策略是这样：**如果存在有效位为0的块，那总是会优先选择这一块去牺牲；如果一组中的块有效位都是1，才会去找时间戳最大的一块**。考虑到这一点，初始化的时候时间戳可以全部初始化为`INT_MAX`。这样每次选择牺牲块的时候，只需要找时间戳最大的一块即可，无需判定有效位。

## 模拟访问缓存
&emsp;&emsp;如果没有hit，至少会有一个miss，根据timestamp的值判断这个块是不是原本是一个有效的数据块，进而判断是否出现eviction块。

&emsp;&emsp;无论命中与否，相应缓存块的timestamp都需要置0，表示最近刚刚被访问过。

&emsp;&emsp;数据修改操作`M`蕴含了一次加载数据`L`和一次存储数据`S`的操作，需要访存两次。

&emsp;&emsp;同时，在`main`函数中，每处理完一个访存操作，要将timestamp中所有有效块的时间戳+1，类似于模拟一个硬件时钟。

```c
static void access_cache(unsigned long long addr, int size)
{
    unsigned long long tag = addr >> (s + b);
    int set_index = (addr >> b) & (S - 1);

    for (int i = 0; i < E; i++)
    {
        /* hit */
        if (valid_bits[set_index][i] && tags[set_index][i] == tag)
        {
            ++hits;
            timestamp[set_index][i] = 0;
            return;
        }
    }

    ++misses;

	/* find max timestamp */ 
    int max_cnt = 0, max_idx = 0;
    for (int i = 0; i < E; i++)
    {
        if (timestamp[set_index][i] > max_cnt)
        {
            max_cnt = timestamp[set_index][i];
            max_idx = i;
        }
    }

    if (max_cnt != 0x7fffffff) ++evictions;  // eviction occurs if the block to be evicted is set valid
    tags[set_index][max_idx] = tag;
    valid_bits[set_index][max_idx] = 1;
    timestamp[set_index][max_idx] = 0;
}


static void cache_timer(void)
{
    for (int i = 0; i < S; i++)
    {
        for (int j = 0; j < E; j++)
        {
            if (valid_bits[i][j])
                ++timestamp[i][j];
        }
    }
}
```

## 运行结果
&emsp;&emsp;终端键入：
```shell
linux> make
linux> ./test-csim
```
&emsp;&emsp;结果如下：
```shell
                        Your simulator     Reference simulator
Points (s,E,b)    Hits  Misses  Evicts    Hits  Misses  Evicts
     3 (1,1,1)       9       8       6       9       8       6  traces/yi2.trace
     3 (4,2,4)       4       5       2       4       5       2  traces/yi.trace
     3 (2,1,4)       2       3       1       2       3       1  traces/dave.trace
     3 (2,1,3)     167      71      67     167      71      67  traces/trans.trace
     3 (2,2,3)     201      37      29     201      37      29  traces/trans.trace
     3 (2,4,3)     212      26      10     212      26      10  traces/trans.trace
     3 (5,1,5)     231       7       0     231       7       0  traces/trans.trace
     6 (5,1,5)  265189   21775   21743  265189   21775   21743  traces/long.trace
    27

TEST_CSIM_RESULTS=27
```

# Part B. Efficient Matrix Transpose
## 32 * 32矩阵
&emsp;&emsp;第一个测试矩阵大小是 32 * 32的。我们先来分析一下，一个int类型数字是4字节，cache中一行32 字节，可以放8个int。

&emsp;&emsp;想要降低不命中次数，需要提高函数的局部性，要么通过修改循环顺序来提高空间局部性，要么通过分块技术来提高时间局部性。

&emsp;&emsp;矩阵转秩操作扫一个的行必然会扫另一个的列，空间局部性提高不了，所以用分块操作来降低不命中率。

&emsp;&emsp;题目已经给定了cache的参数$s = 5,b = 5,E = 1$。那么 cache 的大小就是32组，每组1行， 每行可存储32字节的数据。而int类型为4字节，所以缓存中的每个数据块可以保存8个元素，由于矩阵是行优先存储的，所以相当于保存了$A[0][0]~A[0][7]$，A矩阵转置后$A[0][0]~A[0][7]$对应的位置为$B[0][0]~B[7][0]$，意味着需要8个高速缓存行（B也是行优先访问），分别保存$B[0][0] \sim B[0][7],B[1][0] \sim B[1][7] \dots$。只有这样，每次取出一个Cache，才能得到充分的利用。

&emsp;&emsp;由于32 * 32矩阵中，每一行有32个元素，4个高速缓存行才能存储完矩阵的1行，32个高速缓存行能存8行矩阵。故设置分块大小为8。

&emsp;&emsp;同时为了避免A和B的缓存冲突，使用**循环展开**技术：用8个临时变量一次把矩阵A中一行八个int全部读进来，然后分别存进B矩阵对应的位置，而不是一次循环只操作一个数，从而避免A和B间的抖动冲突。

&emsp;&emsp;具体讲，操作不在对角线上的块时，比如$A[0..7][8..15]$转秩存入$B[8..15][0..7]$上时，前者存于缓存行第$n$行，其中$n = 1 \bmod 4$；而后者存于缓存行第$m$行，其中$m = 0 \bmod 4$，不存在于同一缓存行，所以没有冲突。但是操作对角线上的块的话，A和B就在一个缓存行。反复的读A写B会产生抖动冲突，增加不命中。循环展开就是为了使用局部变量先一次把A全读进来，再统一写入B中，减少不命中。

&emsp;&emsp;代码如下：
```c
if(M == 32 && N == 32)
{
	int i, j, k, v1, v2, v3, v4, v5, v6, v7, v8;
	for (i = 0; i < 32; i += 8)
    {
		for(j = 0; j < 32; j += 8) // traverse the matrix with stride 4
        {
			for(k = i; k < (i + 8); ++k)
			{
				v1 = A[k][j];
				v2 = A[k][j+1];
				v3 = A[k][j+2];
				v4 = A[k][j+3];
				v5 = A[k][j+4];
				v6 = A[k][j+5];
				v7 = A[k][j+6];			
				v8 = A[k][j+7];
				B[j][k] = v1;
				B[j+1][k] = v2;
				B[j+2][k] = v3;
				B[j+3][k] = v4;
				B[j+4][k] = v5;
				B[j+5][k] = v6;
				B[j+6][k] = v7;
				B[j+7][k] = v8;
			}
        }
    }
}
```

&emsp;&emsp;键入`./test-trans -M 32 -N 32`，结果如下：
```shell

Function 0 (2 total)
Step 1: Validating and generating memory traces
Step 2: Evaluating performance (s=5, E=1, b=5)
func 0 (Transpose submission): hits:1765, misses:288, evictions:256

Function 1 (2 total)
Step 1: Validating and generating memory traces
Step 2: Evaluating performance (s=5, E=1, b=5)
func 1 (Simple row-wise scan transpose): hits:869, misses:1184, evictions:1152

Summary for official submission (func 0): correctness=1 misses=288

TEST_TRANS_RESULTS=1:288
```

## 64 * 64矩阵
&emsp;&emsp;现在整个cache只能读进来4行矩阵，所以设置块大小为4。块大小为8会导致访问一个矩阵时，矩阵内第1行和第5行冲突，第2行和第6行冲突...等等。

&emsp;&emsp;代码如下。
```c
if (M == 64 && N == 64)
{
	int i, j, k, v1, v2, v3, v4;
	for (i = 0; i < 64; i += 4)
    {
		for(j = 0; j < 64; j += 4)
        {
			for(k = i; k < (i + 4); ++k)
			{
				v1 = A[k][j];
				v2 = A[k][j+1];
				v3 = A[k][j+2];
				v4 = A[k][j+3];
				B[j][k] = v1;
				B[j+1][k] = v2;
				B[j+2][k] = v3;
				B[j+3][k] = v4;
            }
        }
    }
}
```

&emsp;&emsp;运行结果如下：
```shell

Function 0 (2 total)
Step 1: Validating and generating memory traces
Step 2: Evaluating performance (s=5, E=1, b=5)
func 0 (Transpose submission): hits:6497, misses:1700, evictions:1668

Function 1 (2 total)
Step 1: Validating and generating memory traces
Step 2: Evaluating performance (s=5, E=1, b=5)
func 1 (Simple row-wise scan transpose): hits:3473, misses:4724, evictions:4692

Summary for official submission (func 0): correctness=1 misses=1700

TEST_TRANS_RESULTS=1:1700
```


## 61 * 67矩阵
&emsp;&emsp;很鸡贼的一个样例。不是n * n矩阵，甚至不是4的倍数或者8的倍数，如果分块大小大一些，说不定不会出现上一个样例设置分块大小为8时出现的“矩阵内第1行和第5行冲突，第2行和第6行冲突...”情况。同时分块太小会导致cache利用率底。所以设置了分块大小为8试试看。至于边界则顺序存取，无须特殊处理。

&emsp;&emsp;代码如下：
```c
if(M == 61 && N == 67)
{
	int i, j, v1, v2, v3, v4, v5, v6, v7, v8;
	int n = N / 8 * 8;
	int m = M / 8 * 8;
	for (j = 0; j < m; j += 8)
    {
		for (i = 0; i < n; ++i)
		{
			v1 = A[i][j];
			v2 = A[i][j+1];
			v3 = A[i][j+2];
			v4 = A[i][j+3];
			v5 = A[i][j+4];
			v6 = A[i][j+5];
			v7 = A[i][j+6];
			v8 = A[i][j+7];
				
			B[j][i] = v1;
			B[j+1][i] = v2;
			B[j+2][i] = v3;
			B[j+3][i] = v4;
			B[j+4][i] = v5;
			B[j+5][i] = v6;
			B[j+6][i] = v7;
			B[j+7][i] = v8;
		}
    }

    /* margin of the matrix */
	for (i = n; i < N; ++i)
    {
		for (j = m; j < M; ++j)
		{
			v1 = A[i][j];
			B[j][i] = v1;
		}
    }

	for (i = 0; i < N; ++i)
    {
		for (j = m; j < M; ++j)
		{
			v1 = A[i][j];
			B[j][i] = v1;
		}
    }

	for (i = n; i < N; ++i)
    {
		for (j = 0; j < M; ++j)
		{
			v1 = A[i][j];
			B[j][i] = v1;
		}
    }
}
```

&emsp;&emsp;运行结果如下：
```shell

Function 0 (2 total)
Step 1: Validating and generating memory traces
Step 2: Evaluating performance (s=5, E=1, b=5)
func 0 (Transpose submission): hits:6333, misses:1906, evictions:1874

Function 1 (2 total)
Step 1: Validating and generating memory traces
Step 2: Evaluating performance (s=5, E=1, b=5)
func 1 (Simple row-wise scan transpose): hits:3755, misses:4424, evictions:4392

Summary for official submission (func 0): correctness=1 misses=1906

TEST_TRANS_RESULTS=1:1906
```
