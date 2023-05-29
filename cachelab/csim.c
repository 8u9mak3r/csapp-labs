#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cachelab.h"


static char tracefile[32];
static unsigned long long** tags;
static int** timestamp, **valid_bits;
static int s = 0, b = 0;
static int S = 0, E = 0, B = 0;
static int verbose_flag = 0;
static int hits = 0, misses = 0, evictions = 0;


static void parse_args(int, char*[], char*);
static void print_help(void);
static void init_cache(void);
static void access_cache(unsigned long long, int);
static void cache_timer(void);
static void free_cache(void);



int main(int argc, char* argv[])
{
    parse_args(argc, argv, "hvs:E:b:t:");
    init_cache();

    FILE* fp = fopen(tracefile, "r");
    if (fp != NULL)
    {
        while(1)
        {
            char c = fgetc(fp);

            if (feof(fp)) break;

            if (c == 'I') // ignore if required loading instructor
            {
                while (c != '\n') { c = fgetc(fp); } 
                continue;
            }

            char op;
            fscanf(fp, "%c", &op);

            fgetc(fp); // read off a space

            unsigned long long addr;
            fscanf(fp, "%llx", &addr);

            fgetc(fp); // read off a comma

            int size;
            fscanf(fp, "%d", &size);

            fgetc(fp); // read off a newline character

            int old_hits = hits, old_misses = misses, old_evictions = evictions;

            switch (op)
            {
                case 'M':
                    access_cache(addr, size);
                    access_cache(addr, size);
                    break;

                default:
                    access_cache(addr, size);
                    break;
            }

            if (verbose_flag)
            {
                printf("%c %llx,%d", op, addr, size);
                if (old_misses != misses) printf(" miss");
                if (old_evictions != evictions) printf(" eviction");
                if (old_hits != hits) printf(" hit");
                printf("\n");
            }

            cache_timer();
        }

        free_cache();
        fclose(fp);
    }
    else
    {
        fprintf(stderr, "Failed to open file: %s\n", tracefile);
        exit(-1);
    }

    printSummary(hits, misses, evictions);
    return 0;
}


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


static void init_cache(void)
{
    tags = malloc(S * sizeof(unsigned long long*));
    for (int i = 0; i < S; i++) tags[i] = malloc(E * sizeof(unsigned long long));

    valid_bits = malloc(S * sizeof(int*));
    for (int i = 0; i < S; i++) valid_bits[i] = malloc(E * sizeof(int));

    timestamp = malloc(S * sizeof(int*));
    for (int i = 0; i < S; i++) timestamp[i] = malloc(E * sizeof(int));

    /* access_count[i][j] == 0: invalid cache block */
    /* access_count[i][j] != 0: accessed times */
    for (int i = 0; i < S; i++)
    {
        for (int j = 0; j < E; j++)
        {
            valid_bits[i][j] = 0;
            timestamp[i][j] = 0x7fffffff;
        }
    }
}


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

    int max_cnt = 0, max_idx = 0;
    for (int i = 0; i < E; i++)
    {
        if (timestamp[set_index][i] > max_cnt)
        {
            max_cnt = timestamp[set_index][i];
            max_idx = i;
        }
    }

    if (max_cnt != 0x7fffffff) ++evictions;
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


static void free_cache(void)
{
    for (int i = 0; i < S; i++)
    {
        free(tags[i]);
        free(timestamp[i]);
        free(valid_bits[i]);
    }

    free(tags);
    free(timestamp);
    free(valid_bits);
}
