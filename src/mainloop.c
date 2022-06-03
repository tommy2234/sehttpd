#include <arpa/inet.h>
#include <assert.h>
#include <liburing.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"
#include "io_uring.h"
#include "logger.h"
#include "memory_pool.h"

#define LISTENQ 1024
#define NT 4

static int open_listenfd(int port)
{
    int listenfd, optval = 1;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminate "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval,
                   sizeof(int)) < 0)
        return -1;

    /* Listenfd will be an endpoint for all requests to given port. */
    struct sockaddr_in serveraddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons((unsigned short) port),
        .sin_zero = {0},
    };
    if (bind(listenfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;

    return listenfd;
}

/* TODO: use command line options to specify */
#define PORT 8081
#define WEBROOT "./www"

typedef struct thread_data {
    struct io_uring ring;
    int tid;
    int listenfd;
} thread_data_t;

void *server_loop(void *arg);
bool threads_alive = true;

int main()
{
    /* when a fd is closed by remote, writing to this fd will cause system
     * send SIGPIPE to this process, which exit the program
     */
    if (sigaction(SIGPIPE,
                  &(struct sigaction){.sa_handler = SIG_IGN, .sa_flags = 0},
                  NULL)) {
        log_err("Failed to install sigal handler for SIGPIPE");
        return 0;
    }

    int listenfd = open_listenfd(PORT);
    init_req_pool(NT);

    pthread_t *threads = malloc(NT * sizeof(pthread_t));
    thread_data_t *thread_data = malloc(NT * sizeof(thread_data_t));
    for (int i = 0; i < NT; i++) {
        io_uring_init(&thread_data[i].ring);
        thread_data[i].listenfd = listenfd;
        thread_data[i].tid = i;
        pthread_create(&threads[i], NULL, &server_loop, &thread_data[i]);
    }

    printf("Web server started!\n");

    for (int i = 0; i < NT; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);
    free(thread_data);

    return 0;
}

void *server_loop(void *arg)
{
    thread_data_t *data = arg;
    int listenfd = data->listenfd;
    int tid = data->tid;
    struct io_uring *ring = &data->ring;

    http_request_t *r = get_request(tid);
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    add_accept_request(ring, r, listenfd, (struct sockaddr *) &client_addr,
                       &client_len);

    while (threads_alive) {
        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(ring, &cqe);
        http_request_t *cqe_req = io_uring_cqe_get_data(cqe);
        enum event_types type = cqe_req->event_type;

        if (type == ACCEPT) {
            add_accept_request(ring, r, listenfd,
                               (struct sockaddr *) &client_addr, &client_len);
            int clientfd = cqe->res;
            if (clientfd >= 0) {
                http_request_t *request = get_request(tid);
                init_http_request(request, clientfd, WEBROOT, ring);
                add_read_request(ring, request);
            }
        } else if (type == READ) {
            int read_bytes = cqe->res;
            if (read_bytes <= 0) {
                if (read_bytes < 0)
                    fprintf(stderr, "Async request failed: %s for event: %d\n",
                            strerror(-cqe->res), cqe_req->event_type);
                int ret = http_close_conn(cqe_req);
                assert(ret == 0 && "http_close_conn");
            } else {
                do_request(cqe_req, read_bytes);
            }
        } else if (type == WRITE) {
            int write_bytes = cqe->res;
            if (write_bytes <= 0) {
                if (write_bytes < 0)
                    fprintf(stderr, "Async request failed: %s for event: %d\n",
                            strerror(-cqe->res), cqe_req->event_type);
                int ret = http_close_conn(cqe_req);
                assert(ret == 0 && "http_close_conn");
            } else {
                if (cqe_req->keep_alive == false)
                    http_close_conn(cqe_req);
                else
                    add_read_request(ring, cqe_req);
            }
        } else if (type == TIMEOUT) {
            free_request(cqe_req);
        }
        io_uring_cq_advance(ring, 1);
    }
    io_uring_queue_exit(ring);

    return NULL;
}
