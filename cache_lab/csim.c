#include "cachelab.h"
#include "unistd.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include <getopt.h>

typedef struct Cache_line {
    int valid;
    int tag;
    int timestamp;
} Cache_line;


typedef struct Cache {
    int S; // # number of sets
    int E; // # number of lines per set
    int B; // # data block size (bytes)
    Cache_line **cache_line;
} Cache;


Cache *create_cache(int s, int E, int b) {
    // s -> set index bits
    // E -> number of cache_lines per set
    int S = 1 << s;
    int B = 1 << b;

    Cache *cache = (Cache *)malloc(sizeof(Cache));
    cache->S = S; // 2^s
    cache->E = E;
    cache->B = B; // 2^b

    cache->cache_line = (Cache_line **)malloc(S * sizeof(Cache_line *));
    for (int i = 0; i < S; i++) {
        cache->cache_line[i] = (Cache_line *)malloc(E * sizeof(Cache_line));
        for (int j = 0; j < E; j++) {
            cache->cache_line[i][j].valid = 0;
            cache->cache_line[i][j].tag = 0;
            cache->cache_line[i][j].timestamp = 0;
        }
    }
    return cache;
}

void free_cache(Cache *cache) {
    int S = cache->S;
    for (int i = 0; i < S; i++) {
        free(cache->cache_line[i]);
    }
    free(cache->cache_line);
    free(cache);
}

int hit_count = 0;
int miss_count = 0;
int eviction_count = 0;
Cache *cache = NULL;
int verbose = 0;
char t[1000];

int is_cache_hit(int set_index, int address_tag) {
    Cache_line *set = cache->cache_line[set_index];
    for (int i = 0; i < cache->E; i++) {
        if (set[i].valid && set[i].tag == address_tag) {
            return i; // cache hit, return line index
        }
    }
    return -1; // cache miss
}


int is_full(int set_index) {
    Cache_line *set = cache->cache_line[set_index];
    for (int i = 0; i < cache->E; i++) {
        if (set[i].valid == 0) {
            // not full
            return i; // return index of empty line in the set
        }
    }
    // this set is full
    return -1;
}

// when the cache set is full, call this function
int get_LRU_index(int set_index) {
    int max_time_stamp = 0;
    int LRU_index = 0;

    Cache_line *set = cache->cache_line[set_index];
    for (int i = 0; i < cache->E; i++) {
        if (set[i].valid && set[i].timestamp > max_time_stamp) {
            max_time_stamp = set[i].timestamp;
            LRU_index = i;
        }
    }
    return LRU_index;
}


void update_cache_line(int set_index, int address_tag, int line_index) {
    Cache_line *set = cache->cache_line[set_index];
    set[line_index].valid = 1;
    set[line_index].tag = address_tag;

    for (int i = 0; i < cache->E; i++) {
        set[i].timestamp++;
    }
    set[line_index].timestamp = 0;
}


void access_memory_data(int set_index, int address_tag) {
    // check if there is a cache hit
    int line_index = is_cache_hit(set_index, address_tag);
    if (line_index != -1) { // cache hit
        hit_count++;
        if (verbose) {
            printf(" hit");
        }

        update_cache_line(set_index, address_tag, line_index);
    } else { // cache miss
        miss_count++;
        if (verbose) {
            printf(" miss");
        }

        int empty_line_index = is_full(set_index);
        if (empty_line_index != -1) { // there are empty lines
            update_cache_line(set_index, address_tag, empty_line_index);
        } else { // the cache set is full
            eviction_count++;
            if (verbose) {
                printf(" eviction");
            }

            update_cache_line(set_index, address_tag, get_LRU_index(set_index));
        }
    }
}

void access_memory(int b, int s) {
    FILE *pFile;

    pFile = fopen(t, "r");

    if (pFile == NULL) {
        exit(-1);
    }

    char operation;
    unsigned address;
    int size;

    // read file
    while (fscanf(pFile, " %c %x %d", &operation, &address, &size) > 0) {
        // get set index and tag;
        int address_tag = address >> (b + s);
        int set_index = (address >> b) & (((unsigned) -1) >> (8 * sizeof(unsigned) - s));

        if (verbose) {
            printf("\n %c %x %d", operation, address, size);
        }

        switch (operation) {
            case 'M':
                access_memory_data(set_index, address_tag);
                access_memory_data(set_index, address_tag);
                break;
            case  'L':
                access_memory_data(set_index, address_tag);
                break;
            case 'S':
                access_memory_data(set_index, address_tag);
                break;
        }
    }

    fclose(pFile);

}


void print_usage_info() {

    printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n"
           "• -h: Optional help flag that prints usage info\n"
           "• -v: Optional verbose flag that displays trace info\n"
           "• -s <s>: Number of set index bits (S = 2s is the number of sets)\n"
           "• -E <E>: Associativity (number of lines per set)\n"
           "• -b <b>: Number of block bits (B = 2b is the block size)\n"
           "• -t <tracefile>: Name of the valgrind trace to replay\n");
}


int main(int argc, char **argv)
{
    // parse the command and get operation type and address
    int opt;
    char s, E, b;

    while (-1 != (opt = getopt(argc, argv, "hvs:E:b:t:"))) {
        switch(opt) {
            case 'h':
                print_usage_info();
                exit(0);
                break;
            case 'v':
                verbose = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                strcpy(t, optarg);
                printf(" %s", t);
                break;
            default:
                printf("wrong argument");
                break;
        }

        cache = create_cache(s, E, b);
        access_memory(b, s);
        free_cache(cache);
    }
    printf("%d", hit_count);
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
