#include <npunch/nat.h>
#include <base/prot_array.h>
#include <stdio.h>

int nat_get_type(
    ln_socket *client,
    naddr_t *first_stun,
    naddr_t *second_stun,
    unsigned short port,

    nat_type *type
){
    // ln_socket client;
    // if (0 > ln_usock_new(&client)) return -1;

    naddr_t addr[2] = {0};

    naddr_t n = ln_make4(ln_ipv4("0.0.0.0", port));
    ln_netfd(&n, &client->fd);

    if (0 > bind(
        client->fd.rfd,
        (const struct sockaddr*)&client->fd.addr,
        client->fd.addr_len
    )) {perror("bind"); return -1; }

    if (0 > natp_request_stun(client, first_stun)){
        ln_usock_close(client);
        return -2;
    }
    addr[0] = client->addr;

    if (0 > natp_request_stun(client, second_stun)){
        ln_usock_close(client);
        return -3;
    }
    addr[1] = client->addr;

    // ln_usock_close(client);
    *type = (ln_gport(&addr[0]) == ln_gport(&addr[1])) ? (
        (ln_gport(&addr[0]) == port) ? NAT_STATIC: NAT_DYNAMIC
    ): NAT_SYMMETRIC;

    return 0;
}

const char *strnattype(nat_type type){
    static const char *answer = NULL;
    switch (type){
        case NAT_SYMMETRIC: answer = "SYMMETRIC"; break;
        case NAT_DYNAMIC:   answer = "DYNAMIC";   break;
        case NAT_STATIC:    answer = "STATIC";    break;
        case NAT_ERROR:     answer = "ERROR";     break;
    }
    return answer;
}


typedef struct {
    stun_addr *addresses;
    size_t     n_to_resolve;

    prot_array *out_addrs;
} resolution_args;

static void *_parallel_stun_resolution(void *_args){
    resolution_args *args = _args;

    for (size_t i = 0; i < args->n_to_resolve; i++){
        stun_addr curr = *(args->addresses + i);

        naddr_t out;
        if (0 > ln_uni(curr.uniaddr, curr.port, &out))
            continue;

        prot_array_push(args->out_addrs, &out);
    }

    free(_args);
    return NULL;
}

nat_type nat_parallel_req(ln_socket *client, stun_addr *stuns, size_t stuns_n){
    int paral_res_n = STUN_PARALLEL_RESOLUTION_LIMIT > stuns_n ? stuns_n: STUN_PARALLEL_RESOLUTION_LIMIT;

    prot_array q;
    prot_array_create(sizeof(naddr_t), &q);

    pthread_t threads[paral_res_n];
    for (int i = 0; i < paral_res_n; i++){
        resolution_args *args = malloc(sizeof(*args));
        args->addresses = stuns + i * (paral_res_n / stuns_n);
        args->n_to_resolve = paral_res_n / stuns_n;
        args->out_addrs = &q;

        int r = pthread_create(&threads[i], NULL, _parallel_stun_resolution, args);
        if (0 > r) {
            free(args);
            return NAT_ERROR;
        }
    }

    for (int i = 0; i < paral_res_n; i++)
        pthread_join(threads[i], NULL);

    nat_type type = NAT_ERROR;

    if (prot_array_len(&q) < 2)
        goto end;

    size_t first_i = 0, second_i = 1, len = prot_array_len(&q);
    do {
        uint16_t port = 1024 + (rand() % 63000);

        naddr_t *stun1 = prot_array_at(&q, first_i);
        naddr_t *stun2 = prot_array_at(&q, second_i);

        int r = nat_get_type(client, stun1, stun2, port, &type);
        if (r == 0) break;
        if (r == -1) goto end;

        // first stun failed
        if (r == -2) {
            first_i++;
            if (first_i == second_i)
                first_i++;

            if (first_i >= len)
                goto end;
        }

        // second stun failed
        if (r == -3) {
            second_i++;
            if (second_i == first_i)
                second_i++;

            if (second_i >= len)
                goto end;
        }
    } while(true);

end:

    prot_array_end(&q);
    return type;
}
