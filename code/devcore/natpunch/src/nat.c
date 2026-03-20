#include <npunch/nat.h>
#include <stdio.h>

int nat_get_type(
    ln_usocket *client,
    naddr_t *first_stun,
    naddr_t *second_stun,
    unsigned short port,

    nat_type *type
){
    // ln_usocket client;
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
    *type = (addr[0].ip.v4.port == addr[1].ip.v4.port) ? (
        (addr[0].ip.v4.port == port) ? NAT_STATIC: NAT_DYNAMIC
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