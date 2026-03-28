#ifndef UHTTP_SERVER_H
#define UHTTP_SERVER_H

#include "uhttp/api.h"
#include <stdatomic.h>
#include <sys/poll.h>
#include <uhttp/parser.h>
#include <lownet/tcp_sock.h>
#include <base/prot_queue.h>
#include <multithr/events.h>

typedef struct {
    nnet_fd nfd;
    uhttp_response resp;
} uhttp_high_response;

typedef struct {
    nnet_fd nfd;
    uhttp_request req;
} uhttp_high_request;

typedef struct {
    char path[512];
    uhttp_response (*handler)(uhttp_high_request *req, void *ctx);
} uhttp_route;

typedef struct {
    atomic_bool  is_running;
    ln_socket    sock;
    prot_queue   clis;
    mt_eventsock ev;

    mt_eventsock request_ev;
    prot_queue   requests;

    prot_queue   responses;
    mt_eventsock response_ev;

    pthread_t runner_thr, accepter_thr;

    uhttp_route *routes;
    size_t       routes_n;

    void *ctx;
} uhttp_serv;

typedef struct {
    uhttp_serv *serv_ptr;
    ln_socket   sock;

    void   *read_data;
    size_t  read_size;
    bool    read_done;
} uhttp_cli_state;

int  uhttp_set_route(uhttp_serv *server, const char *path, uhttp_response (*handler)(uhttp_high_request *req, void *ctx));
int  uhttp_server_create(uhttp_serv *server, naddr_t bind_addr, void *ctx);
int  uhttp_server_end   (uhttp_serv *server);

int  uhttp_server_poll_routes(uhttp_serv *server);
int  uhttp_server_run      (uhttp_serv **server);
int  uhttp_server_stop     (uhttp_serv  *server);
bool uhttp_server_isrunning(uhttp_serv  *server);

int uhttp_wait_requests  (uhttp_serv *server, int timeout);
int uhttp_request_iterate(uhttp_serv *server, uhttp_high_request *request);

int uhttp_send_response(uhttp_serv *server, uhttp_response *resp, uhttp_high_request *answer_to);

#endif
