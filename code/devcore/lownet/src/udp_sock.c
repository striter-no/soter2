#include <lownet/udp_sock.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int ln_usock_new(ln_socket *sck){
    if (!sck) return -1;
    sck->fd.rfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    if (sck->fd.rfd < 0) return -1;

    int opt = 1;
    if (0 > setsockopt(sck->fd.rfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
        perror("setsockopt");
        return -1;
    }
    if (0 > setsockopt(sck->fd.rfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))){
        perror("setsockopt");
        return -1;
    }

    return 0;
}

void ln_usock_close(ln_socket *sck){
    if (!sck) return;
    close(sck->fd.rfd);
}

int ln_usock_bind(ln_socket *sck, naddr_t addr){
    if (!sck) return -1;
    if (0 > ln_netfd(&addr, &sck->fd)) return -1;
    sck->addr = addr;

    return bind(
        sck->fd.rfd,
        (const struct sockaddr*)&sck->fd.addr,
        sck->fd.addr_len
    );
}

ssize_t ln_usock_recv(ln_socket *sck, void *buf, size_t n, nnet_fd *from){
    if (!sck) return -1;

    if (from) {
        from->addr_len = sizeof(struct sockaddr_storage);
        memset(&from->addr, 0, sizeof(from->addr));
    }

    return recvfrom(
        sck->fd.rfd,
        buf, n, 0,
        (from ? (struct sockaddr*)&from->addr: NULL),
        (from ? &from->addr_len: NULL)
    );
}

ssize_t ln_usock_send(ln_socket *sck, const void *buf, size_t n, const nnet_fd *to){
    if (!sck) return -1;

    return sendto(
        sck->fd.rfd,
        buf, n, 0,
        (const struct sockaddr*)&to->addr,
        to->addr_len
    );
}
