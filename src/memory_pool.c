#include "memory_pool.h"

#define POOL_SZ 8192
#define BITMAP_SZ POOL_SZ / 32
uint32_t req_bitmap[BITMAP_SZ];
uint32_t job_bitmap[BITMAP_SZ];
http_request_t *req_pool;
job *job_pool;
pthread_mutex_t job_pool_lock;
pthread_mutex_t req_pool_lock;

int init_req_pool()
{
    pthread_mutex_init(&req_pool_lock, NULL);
    req_pool = malloc(POOL_SZ * sizeof(http_request_t));
    for (int i = 0; i < POOL_SZ; i++) {
        if (!&req_pool[i]) {
            printf("Memory %d calloc fail\n", i);
            exit(1);
        }
        (&req_pool[i])->pool_id = i;
    }
    return 0;
}

int init_job_pool()
{
    pthread_mutex_init(&job_pool_lock, NULL);
    job_pool = malloc(POOL_SZ * sizeof(job));
    for (int i = 0; i < POOL_SZ; i++) {
        if (!&job_pool[i]) {
            printf("Memory %d calloc fail\n", i);
            exit(1);
        }
        (&job_pool[i])->pool_id = i;
    }
    return 0;
}

inline http_request_t *get_request()
{
    pthread_mutex_lock(&req_pool_lock);
    for (int i = 0; i < BITMAP_SZ; i++) {
        uint32_t bitset = req_bitmap[i];
        if (!(bitset ^ 0xffffffff))
            continue;

        for (int k = 0; k < 32; k++) {
            if (!((bitset >> k) & 0x1)) {
                req_bitmap[i] ^= (0x1 << k);
                int pos = 32 * i + k;
                pthread_mutex_unlock(&req_pool_lock);
                return &req_pool[pos];
            }
        }
    }
    printf("Too much client connected!\n");
    pthread_mutex_unlock(&req_pool_lock);
    return NULL;
}

inline job *get_job()
{
    pthread_mutex_lock(&job_pool_lock);
    for (int i = 0; i < BITMAP_SZ; i++) {
        uint32_t bitset = job_bitmap[i];
        if (!(bitset ^ 0xffffffff))
            continue;

        for (int k = 0; k < 32; k++) {
            if (!((bitset >> k) & 0x1)) {
                job_bitmap[i] ^= (0x1 << k);
                int pos = 32 * i + k;
                pthread_mutex_unlock(&job_pool_lock);
                return &job_pool[pos];
            }
        }
    }
    printf("Too much request!\n");
    pthread_mutex_unlock(&job_pool_lock);
    return NULL;
}

int free_request(http_request_t *req)
{
    pthread_mutex_lock(&req_pool_lock);
    int pos = req->pool_id;
    req_bitmap[pos / 32] ^= (0x1 << (pos % 32));
    pthread_mutex_unlock(&req_pool_lock);
    return 0;
}

int free_job(job *j)
{
    pthread_mutex_lock(&job_pool_lock);
    int pos = j->pool_id;
    job_bitmap[pos / 32] ^= (0x1 << (pos % 32));
    pthread_mutex_unlock(&job_pool_lock);
    return 0;
}
