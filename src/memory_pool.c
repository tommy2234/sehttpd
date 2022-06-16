#include "memory_pool.h"

#define POOL_SZ 8192

http_request_t **req_pool;
int *next;

int init_req_pool(int nt)
{
    req_pool = malloc(nt * POOL_SZ * sizeof(http_request_t *));
    for (int i = 0; i < nt * POOL_SZ; i++) {
        req_pool[i] = malloc(sizeof(http_request_t));
        req_pool[i]->tid = i / POOL_SZ;
    }
    next = calloc(nt, sizeof(int));

    return 0;
}

http_request_t *get_request(int tid)
{
    if (next[tid] == POOL_SZ) {
        printf("Too much client connected!\n");
        return NULL;
    }
    int idx = POOL_SZ * tid + next[tid];
    next[tid]++;
    return req_pool[idx];
}

int free_request(http_request_t *r)
{
    next[r->tid]--;
    int idx = POOL_SZ * r->tid + next[r->tid];
    req_pool[idx] = r;
    return 0;
}
