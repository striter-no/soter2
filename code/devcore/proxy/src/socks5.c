#include <netinet/in.h>
#include <proxy/socks5.h>
#include <proxy/socks5_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>

int __stas5_recv_string(ln_socket *sock, char *str) {
    uint8_t len;
    if (ln_tsock_readx(sock, sizeof(uint8_t), &len, -1) <= 0) {
        return -1;
    }
    if (ln_tsock_readx(sock, len, str, -1) <= 0) {
        return -1;
    }
    str[len] = '\0';
    return len;
}

void __stas5_send_server_hello(ln_socket *sock, uint8_t method) {
    // printf("[stas5] sending server HELLO...\n");
    socks5_server_hello_t server_hello = {
            .version = SOCKS5_VERSION,
            .method = method,
    };
    ln_tsock_writex(sock, sizeof(socks5_server_hello_t), &server_hello, -1);
}

int __stas5_handle_greeting(ln_socket *sock) {
    // printf("[stas5] handle greeting...\n");
    if (!sock) {
        return -1;
    }

    socks5_client_hello_t client_hello;

    if (ln_tsock_readx(sock, sizeof(socks5_client_hello_t), &client_hello, -1) <= 0) {
        fprintf(stderr, "[proxy][S5] network failure\n");
        return -1;
    }

    if (client_hello.version != SOCKS5_VERSION) {
        fprintf(stderr, "[proxy][S5] unsupported socks version %#02x\n", client_hello.version);
        return -1;
    }

    uint8_t methods[UINT8_MAX];
    if (ln_tsock_readx(sock, client_hello.num_methods, methods, -1) <= 0) {
        return -1;
    }

    // Find server auth method in client's list
    int found = 0;
    for (int i = 0; i < (int) client_hello.num_methods; i++) {
        if (methods[i] == SOCKS5_AUTH_NO_AUTH) {
            // Find auth method in client's supported method list
            found = 1;
            break;
        }
    }

    if (!found) {
        // No acceptable method
        fprintf(stderr, "[proxy][S5] No acceptable method from client\n");
        __stas5_send_server_hello(sock, SOCKS5_AUTH_NOT_ACCEPT);
        return -1;
    }
    // Send auth method choice
    __stas5_send_server_hello(sock, SOCKS5_AUTH_NO_AUTH);
    return 0;
}

void __stas5_send_domain_reply(ln_socket *sock, uint8_t reply_type, const char *domain, uint8_t domain_len, in_port_t port) {
    uint8_t buffer[sizeof(uint8_t) + UINT8_MAX + sizeof(in_port_t)];
    uint8_t *ptr = buffer;
    *(socks5_reply_t *) ptr = (socks5_reply_t) {
            .version = SOCKS5_VERSION,
            .reply = reply_type,
            .reserved = 0,
            .addr_type = SOCKS5_ATYP_DOMAIN_NAME
    };
    ptr += sizeof(socks5_reply_t);
    *ptr = domain_len;
    ptr += sizeof(uint8_t);
    memcpy(ptr, domain, domain_len);
    ptr += domain_len;
    *(in_port_t *) ptr = port;
    ptr += sizeof(in_port_t);
    ln_tsock_writex(sock, ptr - buffer, buffer, -1);
}

void __stas5_send_ip_reply(ln_socket *sock, uint8_t reply_type, in_addr_t ip, in_port_t port) {
    uint8_t buffer[sizeof(socks5_reply_t) + sizeof(in_addr_t) + sizeof(in_port_t)];
    uint8_t *ptr = buffer;
    *(socks5_reply_t *) ptr = (socks5_reply_t) {
            .version = SOCKS5_VERSION,
            .reply = reply_type,
            .reserved = 0,
            .addr_type = SOCKS5_ATYP_IPV4
    };
    ptr += sizeof(socks5_reply_t);
    *(in_addr_t *) ptr = ip;
    ptr += sizeof(in_addr_t);
    *(in_port_t *) ptr = port;
    ln_tsock_writex(sock, sizeof(buffer), buffer, -1);
}

int prx_socks5_handle_request(ln_socket *sock, s5_uni_addr *out_addr) {
    // printf("[proxy][S5] handle request\n");
    // Handle socks request
    socks5_request_t req;

    if (ln_tsock_readx(sock, sizeof(socks5_request_t), &req, -1) <= 0) {
        return -1;
    }

    if (req.version != SOCKS5_VERSION) {
        fprintf(stderr, "[proxy][S5] unsupported socks version %#02x\n", req.version);
        return -1;
    }
    if (req.command != SOCKS5_CMD_CONNECT) {
        fprintf(stderr, "[proxy][S5] unsupported command %#02x\n", req.command);
        return -1;
    }


    if (req.addr_type == SOCKS5_ATYP_IPV4) {
        in_addr_t ip;
        if (ln_tsock_readx(sock, sizeof(in_addr_t), &ip, -1) <= 0)
            return -1;

        in_port_t port;
        if (ln_tsock_readx(sock, sizeof(in_port_t), &port, -1) <= 0)
            return -1;

        inet_ntop(AF_INET, &ip, out_addr->uni.ip.ip.v4.ip, INET_ADDRSTRLEN);
        out_addr->uni.ip.ip.v4.port = ntohs(port);
        out_addr->uni.ip.t = nADDR_IPV4;
        out_addr->type = _S5_IP_ADDR;
        return 0;
    } else if (req.addr_type == SOCKS5_ATYP_DOMAIN_NAME) {
        // Get domain name
        char domain[UINT8_MAX + 1];
        int domain_len = __stas5_recv_string(sock, domain);
        if (domain_len <= 0) {
            return -1;
        }
        // Get port
        in_port_t port;
        if (ln_tsock_readx(sock, sizeof(in_port_t), &port, -1) <= 0) {
            return -1;
        }

        strcpy(out_addr->uni.domain.domain, domain);
        out_addr->uni.domain.port = ntohs(port);
        out_addr->uni.domain.domain_len = domain_len;
        out_addr->type = _S5_DOMAIN_ADDR;
        return 0;
    }

    fprintf(stderr, "[proxy][S5] unsupported address type %#02x\n", req.addr_type);
    return -1;
}


// * resolves domain-typed addresses
// * performs connection to the address, returns connected socket
int prx_resolve_address(s5_uni_addr address, ln_socket *sock){
    // printf("[prx] resolving address\n");
    if (!sock) return -1;

    switch (address.type){
        case _S5_IP_ADDR: {
            printf("[prx] ip addr: %s:%u\n", ln_gip(&address.uni.ip), ln_gport(&address.uni.ip));
            sock->addr = address.uni.ip;
            ln_tsock_new(sock);
            if (ln_tsock_connectx(sock, address.uni.ip, -1) <= 0) {
                perror("[resolve] connect failed");
                return -1;
            }

        } break; // IP Address
        case _S5_DOMAIN_ADDR: {
            // printf("[prx] domain\n");
            _s5_domain_address addr = address.uni.domain;

            struct addrinfo hints = {0}, *addr_info, *ai;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            char port_s[8];
            sprintf(port_s, "%d", addr.port);

            if (getaddrinfo(addr.domain, port_s, &hints, &addr_info) != 0) {
                perror("getaddrinfo()");
                return -1;
            }

            int connected = 0;
            for (ai = addr_info; ai != NULL; ai = ai->ai_next) {
                ln_tsock_new(sock);

                naddr_t target = {0};
                if (ai->ai_family == AF_INET) {
                    struct sockaddr_in *sin = (struct sockaddr_in*)ai->ai_addr;
                    target = ln_from_uint32(sin->sin_addr.s_addr, ntohs(sin->sin_port));
                }

                if (ln_tsock_connectx(sock, target, 5000) == 0) {
                    connected = 1;
                    break;
                }
                ln_tsock_close(sock);
            }
            freeaddrinfo(addr_info);

            if (!connected) {
                fprintf(stderr, "[proxy][S5] cannot connect to %s:%d\n", addr.domain, addr.port);
                return -1;
            }
        } break;

        default: return -1;
    }

    return 0;
}

struct __prx_socks5_wrapper_args {
    int (*wrapped)(ln_socket* client, ln_socket* remote, int who, void *state_holder);
    void (*connection_handler)(ln_socket* client, s5_uni_addr rem_address, ln_socket *output_socket, void *state_holder);
    ln_socket client;
    atomic_bool *is_active;
    void *state_holder;
};

void *__prx_socks5_tunnel_wrapper(void *_args){
    // printf("[prx][s5] tunnel wrapper created\n");
    struct __prx_socks5_wrapper_args *args = _args;

    if (__stas5_handle_greeting(&args->client) != 0){
        fprintf(stderr, "[proxy][S5] greeting not handled\n");
        ln_tsock_close(&args->client);
        free(args);
        return NULL;
    }

    s5_uni_addr raddr;
    if (0 > prx_socks5_handle_request(&args->client, &raddr)){
        fprintf(stderr, "[proxy][S5] request not handled\n");
        goto err;
    }

    ln_socket remote;
    args->connection_handler(&args->client, raddr, &remote, args->state_holder);
    if (raddr.type == _S5_IP_ADDR){
        __stas5_send_ip_reply(
            &args->client,
            remote.fd.rfd >= 0 ? SOCKS5_REP_SUCCESS: SOCKS5_REP_GENERAL_FAILURE,
            inet_addr(raddr.uni.ip.ip.v4.ip),
            htons(raddr.uni.ip.ip.v4.port)
        );
    } else {
        __stas5_send_domain_reply(
            &args->client,
            remote.fd.rfd >= 0 ? SOCKS5_REP_SUCCESS: SOCKS5_REP_GENERAL_FAILURE,
            raddr.uni.domain.domain,
            raddr.uni.domain.domain_len,
            htons(raddr.uni.domain.port)
        );
    }

    if (remote.fd.rfd <= 0) {
        fprintf(stderr, "[proxy][S5] connection not handled\n");
        goto err;
    }
    struct pollfd fds[2] = {
        {.fd = args->client.fd.rfd, .events = POLLIN},
        {.fd = remote.fd.rfd,        .events = POLLIN}
    };

    while (atomic_load(args->is_active)){
        int ret = poll(fds, 2, 100);
        if (ret == -1) {
            perror("tunnel_wrapper:poll()");
            break;
        }


        if (ret == 0) continue;
        for (size_t i = 0; i < 2; i++){
            if (fds[i].revents & POLLIN){
                if (0 > args->wrapped(&args->client, &remote, i, args->state_holder)){
                    goto tunbreak;
                }
            } else if (fds[i].revents & (POLLERR | POLLHUP)){
                // printf("POLLERR | POLLHUP\n");
                goto tunbreak;
            }
        }

        continue;
        tunbreak: break;
    }

err:
    ln_tsock_close(&remote);
    ln_tsock_close(&args->client);
    free(args);
    return NULL;
}

// * blocking `while(working)`
// * spawns detached threads with `tunnel_loop`
// * place it into the task if you have any logic
//   after accept loop and outside the connection logic
int prx_socks5_accept_loop(
    ln_socket *proxy,
    int (*tunnel_iter)(ln_socket* client, ln_socket* remote, int who, void *state_holder),
    void (*connection_handler)(ln_socket* client, s5_uni_addr rem_address, ln_socket* output_socket, void *state_holder),
    void *state_holder,
    atomic_bool *is_active
){

    while (atomic_load(is_active)){
        ln_socket client;
        client.fd.addr_len = sizeof(struct sockaddr_in);

        int r = ln_wait_netfd(&proxy->fd, POLLIN, 100);
        if (r == 0) continue;
        if (r < 0) break;

        if (0 > ln_tsock_accept4(proxy, &client)){
            // perror("prx_socks5_accept_loop:prx_nserv_accept()");
            continue;
        }

        // printf("accepted\n");
        if (0 > ln_setopt(&client, IPPROTO_TCP, TCP_NODELAY, 1)){
            perror("prx_socks5_accept_loop:ln_setopt()");
            continue;
        }

        pthread_t client_tid;
        struct __prx_socks5_wrapper_args *args = malloc(sizeof(struct __prx_socks5_wrapper_args));
        if (args == NULL){
            perror("prx_socks5_accept_loop:malloc()");
            continue;
        }

        *args = (struct __prx_socks5_wrapper_args){
            .client = client,
            .wrapped = tunnel_iter,
            .connection_handler = connection_handler,
            .is_active = is_active,
            .state_holder = state_holder
        };

        // printf("creating thread\n");
        if (pthread_create(
            &client_tid,
            NULL,
            &__prx_socks5_tunnel_wrapper,
            args
        ) != 0) {
            perror("pthread_create()");
            ln_tsock_close(&client);
            free(args);
            continue;
        }

        pthread_detach(client_tid);
    }

    return 0;
}
