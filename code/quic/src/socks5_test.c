#include <errno.h>
#include <proxy/socks5.h>
#include <stdio.h>

void connection_handler(
    ln_socket *client_socket,
    s5_uni_addr remote_address,
    ln_socket *out_remote,
    void *state_holder
){
    (void)state_holder;
    (void)client_socket;

    if (0 > prx_resolve_address(remote_address, out_remote)){
        out_remote->fd.rfd = -1;
    }
}

int tunnel_iter(
    ln_socket* client_socket,
    ln_socket* remote_socket,
    int who,
    void *state_holder
){
    (void)state_holder;

    char buffer[BUFSIZ] = {0};
    ln_socket* sockets[2] = {client_socket, remote_socket};

    ssize_t len = read(sockets[who]->fd.rfd, buffer, BUFSIZ);
    if (len <= 0) {
        if (len < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return 0;
        return -1;
    }

    if (write(sockets[1 - who]->fd.rfd, buffer, len) != len) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return -1;
    }
    return 0;
}

int main(void){
    ln_socket proxy;
    ln_tsock_new(&proxy);
    ln_tsock_bind(&proxy, ln_uniq("127.0.0.1", 9000));
    ln_tsock_listen(&proxy, 10);

    atomic_bool is_active;
    atomic_store(&is_active, true);

    prx_socks5_accept_loop(&proxy, tunnel_iter, connection_handler, NULL, &is_active);
    ln_tsock_close(&proxy);
}
