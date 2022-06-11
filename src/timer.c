#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "logger.h"
#include "timer.h"

#define TIMER_INFINITE (-1)
#define PQ_DEFAULT_SIZE 10

typedef int (*prio_queue_comparator)(void *pi, void *pj);

/* priority queue with binary heap */
typedef struct {
    void **priv;  // array of timernode *
    size_t nalloc;
    size_t size;
    prio_queue_comparator comp;
} prio_queue_t;

pthread_mutex_t timer_lock;
int nalloc;

static bool prio_queue_init(struct list_head *prio_queue)
{
    INIT_LIST_HEAD(prio_queue);
    nalloc = 0;
    return true;
}

static inline bool prio_queue_is_empty(struct list_head *prio_queue)
{
    return list_empty(prio_queue);
}

static inline size_t prio_queue_size()
{
    return nalloc;
}

static inline void *prio_queue_min(struct list_head *prio_queue)
{
    return prio_queue_is_empty(prio_queue)
               ? NULL
               : list_entry(prio_queue->next, timer_node, link);
}

/* remove the item with minimum key value from the heap */
static bool prio_queue_delmin(struct list_head *prio_queue)
{
    if (prio_queue_is_empty(prio_queue))
        return true;

    list_del(prio_queue->next);
    nalloc--;
    return true;
}

/* add a new item to the heap */
static bool prio_queue_insert(timer_node *item, struct list_head *prio_queue)
{
    struct list_head *curr, *next;
    list_for_each_safe (curr, next, prio_queue) {
        timer_node *node = list_entry(curr, timer_node, link);
        if (item->key <= node->key || curr == prio_queue)
            list_insert(&item->link, &node->link);
    }
    nalloc++;

    return true;
}

/* FIXME: segmentation fault in multithread environment */
static int timer_comp(void *ti, void *tj)
{
    return ((timer_node *) ti)->key < ((timer_node *) tj)->key ? 1 : 0;
}

static struct list_head timers;
static size_t current_msec;

static void time_update()
{
    struct timeval tv;
    int rc UNUSED = gettimeofday(&tv, NULL);
    assert(rc == 0 && "time_update: gettimeofday error");
    current_msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int timer_init()
{
    pthread_mutex_init(&timer_lock, NULL);
    bool ret UNUSED = prio_queue_init(&timers);
    assert(ret && "prio_queue_init error");

    time_update();
    return 0;
}

int find_timer()
{
    int time = TIMER_INFINITE;

    while (!prio_queue_is_empty(&timers)) {
        time_update();
        timer_node *node = prio_queue_min(&timers);
        assert(node && "prio_queue_min error");

        if (node->deleted) {
            bool ret UNUSED = prio_queue_delmin(&timers);
            assert(ret && "prio_queue_delmin");
            // free(node);
            continue;
        }

        time = (int) (node->key - current_msec);
        time = (time > 0 ? time : 0);
        break;
    }

    return time;
}

void handle_expired_timers()
{
    bool ret UNUSED;

    while (!prio_queue_is_empty(&timers)) {
        debug("handle_expired_timers, size = %zu", prio_queue_size());
        time_update();
        timer_node *node = prio_queue_min(&timers);
        assert(node && "prio_queue_min error");

        if (node->deleted) {
            ret = prio_queue_delmin(&timers);
            assert(ret && "handle_expired_timers: prio_queue_delmin error");
            // free(node);
            continue;
        }

        if (node->key > current_msec) {
            return;
        }
        if (node->callback)
            node->callback(node->request);

        ret = prio_queue_delmin(&timers);
        assert(ret && "handle_expired_timers: prio_queue_delmin error");
        free(node);
    }
}

void add_timer(http_request_t *req, size_t timeout, timer_callback cb)
{
    timer_node *node = malloc(sizeof(timer_node));
    assert(node && "add_timer: malloc error");

    time_update();
    req->timer = node;
    node->key = current_msec + timeout;
    node->deleted = false;
    node->callback = cb;
    node->request = req;

    bool ret UNUSED = prio_queue_insert(node, &timers);
    assert(ret && "add_timer: prio_queue_insert error");
}

void del_timer(http_request_t *req)
{
    time_update();
    timer_node *node = req->timer;
    assert(node && "del_timer: req->timers is NULL");

    node->deleted = true;
    list_del(&node->link);
}

void rearm_timer(http_request_t *req)
{
    time_update();
    timer_node *node = req->timer;
    node->deleted = false;
    node->key = current_msec + TIMEOUT_DEFAULT;
    bool ret UNUSED = prio_queue_insert(node, &timers);
    assert(ret && "add_timer: prio_queue_insert error");
}
