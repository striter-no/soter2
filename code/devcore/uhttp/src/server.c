#include <uhttp/api.h>
#include <uhttp/server.h>
#include <asm-generic/errno-base.h>
#include <errno.h>

bool handler(uhttp_cli_state *st){
    if (!st) return true;

    read_processor:
    if (st->read_done) {

        uhttp_request req;
        if (UHTTP_PARSE_OK != uhttp_parse_request(st->read_data, st->read_size, &req)){
            fprintf(stderr, "[uhttp][handler][%s:%d] error while parsing request\n", ln_gip(&st->sock.addr), ln_gport(&st->sock.addr));
            return false;
        }

        uhttp_high_request usreq = {
            .nfd = st->sock.fd,
            .req = req
        };

        prot_queue_push(&st->serv_ptr->requests, &usreq);
        mt_evsock_notify(&st->serv_ptr->request_ev);

        free(st->read_data);
        st->read_done = false;
        st->read_data = NULL;
        st->read_size = 0;
    } else {
        unsigned char buf[2048];
        ssize_t rr = ln_tsock_read(&st->sock, buf, 2048);

        if (rr == -1 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
            return false;

        if (rr == 0)
            return true; // deleting client

        void *n_data = realloc(st->read_data, st->read_size + rr);
        if (!n_data) {
            perror("realloc");
            return true;
        }

        memcpy((unsigned char *)n_data + st->read_size, buf, rr);
        st->read_size += rr;
        st->read_data = n_data;

        if (rr < 2048){ // done reading
            st->read_done = true;
            goto read_processor;
        }
    }

    return false;
}

void *runner(void *_args){
    uhttp_serv *serv = _args;

    dyn_array clients = dyn_array_create(sizeof(uhttp_cli_state));
    while (atomic_load(&serv->is_running)) {
        int r = mt_evsock_wait(&serv->ev, 50);
        if (r > 0){
            mt_evsock_drain(&serv->ev);
            ln_socket sock;

            while (0 == prot_queue_pop(&serv->clis, &sock)){
                uhttp_cli_state st = {
                    .serv_ptr = serv,
                    .read_data = NULL,
                    .read_size = 0,
                    .sock = sock,
                    .read_done = false
                };

                dyn_array_push(&clients, &st);
            }
        }

        if (mt_evsock_wait(&serv->response_ev, 0) > 0) {
            uhttp_high_response resp = {0};

            while (0 == prot_queue_pop(&serv->responses, &resp)){
                ln_socket sock = {
                    .addr = (naddr_t){0},
                    .fd = resp.nfd
                };

                unsigned char *data; size_t sz;
                uhttp_build_response(&resp.resp, &data, &sz);
                uhttp_free_response(&resp.resp);

                ln_tsock_writex(&sock, data, sz, 500);
                free(data);
            }
        }

        for (size_t i = 0; i < clients.len;){
            uhttp_cli_state *cli = dyn_array_at(&clients, i);
            if (ln_wait_netfd(&cli->sock.fd, POLLIN, 10) <= 0) continue;

            if (handler(cli)){
                ln_tsock_close(&cli->sock);
                dyn_array_remove(&clients, i);
                continue;
            }

            i++;
        }
    }

    dyn_array_end(&clients);
    return NULL;
}

void *accepter(void *_args){
    uhttp_serv *serv = _args;

    while (atomic_load(&serv->is_running)){
        ln_wait_netfd(&serv->sock.fd, POLLIN, 500);

        ln_socket cli;
        cli.fd.addr_len = sizeof(struct sockaddr_in);
        if (0 > ln_tsock_accept4(&serv->sock, &cli)){
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                continue;
            else {
                perror("[uhttp][failed][acc] accept4: ");
                break;
            }
        }

        prot_queue_push(&serv->clis, &cli);
        mt_evsock_notify(&serv->ev);
    }

    return NULL;
}

int uhttp_server_create(uhttp_serv *server, naddr_t bind_addr, void *ctx){
    if (!server) return -1;

    ln_tsock_new(&server->sock);
    ln_tsock_bind(&server->sock, bind_addr);
    ln_tsock_listen(&server->sock, 20);

    prot_queue_create(sizeof(ln_socket), &server->clis);
    prot_queue_create(sizeof(uhttp_high_request), &server->requests);
    prot_queue_create(sizeof(uhttp_high_response), &server->responses);
    mt_evsock_new(&server->ev);
    mt_evsock_new(&server->response_ev);
    mt_evsock_new(&server->request_ev);

    atomic_store(&server->is_running, false);

    server->routes = NULL;
    server->routes_n = 0;
    server->ctx = ctx;
    return 0;
}

int uhttp_server_run(uhttp_serv **server_){

    uhttp_serv *server = *server_;
    atomic_store(&server->is_running, true);

    if (0 > pthread_create(&server->runner_thr, NULL, runner, *server_)){
        atomic_store(&server->is_running, false);
        return -1;
    }

    if (0 > pthread_create(&server->accepter_thr, NULL, accepter, *server_)){
        atomic_store(&server->is_running, false);
        return -1;
    }

    return 0;
}

int uhttp_server_end(uhttp_serv *server){
    if (!server) return -1;

    pthread_join(server->runner_thr, NULL);
    pthread_join(server->accepter_thr, NULL);

    uhttp_high_request req;
    while (0 == prot_queue_pop(&server->requests,&req)){
        uhttp_free_request(&req.req);
    }

    uhttp_high_response resp;
    while (0 == prot_queue_pop(&server->requests, &resp)){
        uhttp_free_response(&resp.resp);
    }

    prot_queue_end(&server->requests);
    prot_queue_end(&server->responses);
    prot_queue_end(&server->clis);

    ln_tsock_close (&server->sock);
    mt_evsock_close(&server->ev);

    return 0;
}

int uhttp_server_stop(uhttp_serv *server){
    if (!server) return -1;

    atomic_store(&server->is_running, false);
    return 0;
}

bool uhttp_server_isrunning(uhttp_serv *server){
    if (!server) return -1;

    return atomic_load(&server->is_running);
}

int uhttp_wait_requests(uhttp_serv *server, int timeout){
    if (!server) return -1;

    int r = mt_evsock_wait(&server->request_ev, timeout);
    if (0 > r)
        mt_evsock_drain(&server->request_ev);

    return r;
}

int uhttp_request_iterate(uhttp_serv *server, uhttp_high_request *request){
    if (!server) return -1;

    return prot_queue_pop(&server->requests, request);
}


int uhttp_send_response(uhttp_serv *server, uhttp_response *resp, uhttp_high_request *answer_to){
    if (!server) return -1;

    uhttp_high_response us_resp = {
        .nfd = answer_to->nfd,
        .resp = *resp
    };

    prot_queue_push(&server->responses, &us_resp);
    mt_evsock_notify(&server->response_ev);
    uhttp_free_request(&answer_to->req);

    return 0;
}

int uhttp_set_route(
    uhttp_serv *server,
    const char *path,
    uhttp_response (*handler)(uhttp_high_request *req, void *ctx)
){
    if (!server || !handler || strlen(path) == 0) return -1;

    uhttp_route *new_routes = realloc(server->routes, sizeof(uhttp_route) * (server->routes_n + 1));
    if (!new_routes) return -1;

    new_routes[server->routes_n].handler = handler;
    strcpy(new_routes[server->routes_n].path, path);

    server->routes = new_routes;
    server->routes_n++;
    return 0;
}

int uhttp_server_poll_routes(uhttp_serv *server){
    if (!server) return -1;

    uhttp_serv **ptr = &server;
    uhttp_server_run(ptr);

    while (uhttp_server_isrunning(server)){
        int r = uhttp_wait_requests(server, 500);
        if (r <= 0) continue;

        uhttp_high_request req;
        while (0 == uhttp_request_iterate(server, &req)){
            uhttp_response resp;

            printf("[uhttp][poller] %s request \"%s\"\n", uhttp_str_method(req.req.method), req.req.path);

            bool skip = true;
            for (size_t i = 0; i < server->routes_n; i++){
                uhttp_route *route = &server->routes[i];

                if (strcmp(req.req.path, route->path) == 0){
                    resp = route->handler(&req, server->ctx);
                    skip = false;
                    break;
                }
            }

            if (skip){
                uhttp_free_request(&req.req);
                continue;
            }

            uhttp_send_response(server, &resp, &req);
        }
    }

    return 0;
}
