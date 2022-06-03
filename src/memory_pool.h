#ifndef _MEM_POOL_
#define _MEM_POOL_

//#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "http.h"

int init_req_pool(int nt);
http_request_t *get_request(int tid);
int free_request(http_request_t *req);

#endif
