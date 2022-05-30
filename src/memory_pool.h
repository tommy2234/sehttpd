#ifndef _MEM_POOL_
#define _MEM_POOL_

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "http.h"
#include "thread_pool.h"

int init_req_pool();
int init_job_pool();
http_request_t *get_request();
job *get_job();
int free_request(http_request_t *req);
int free_job(job *req);

#endif
