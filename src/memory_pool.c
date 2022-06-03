#include "memory_pool.h"

#define POOL_SZ 8192
#define BITMAP_SZ POOL_SZ / 32

uint32_t *bitmap;
http_request_t *req_pool;
// pthread_mutex_t req_pool_lock;

int init_req_pool(int nt)
{
    // pthread_mutex_init(&req_pool_lock, NULL);
    req_pool = malloc(nt * POOL_SZ * sizeof(http_request_t));
    bitmap = malloc(nt * BITMAP_SZ * sizeof(uint32_t));
    for (int i = 0; i < nt * POOL_SZ; i++) {
        req_pool[i].pool_id = i;
        req_pool[i].tid = i / POOL_SZ;
    }
    for (int i = 0; i < nt * BITMAP_SZ; i++)
        bitmap[i] |= 0xffffffff;

    return 0;
}

inline http_request_t *get_request(int tid)
{
    int start = tid * BITMAP_SZ;
    int end = start + BITMAP_SZ;
    for (int i = start; i < end; i++) {
        uint32_t bitset = bitmap[i];
        if (bitset == 0)
            continue;

        int shift = __builtin_ffs(bitset) - 1;
        bitmap[i] ^= (0x1 << shift);
        int pos = 32 * i + shift;
        return &req_pool[pos];
    }
    printf("Too much client connected!\n");
    return NULL;
}

int free_request(http_request_t *req)
{
    int pos = req->pool_id;
    bitmap[pos / 32] ^= (0x1 << (pos % 32));
    return 0;
}
