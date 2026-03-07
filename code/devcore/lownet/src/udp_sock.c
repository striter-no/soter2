#include <lownet/udp_sock.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int ln_usock_new(ln_usocket *cli){
    if (!cli) return -1;
    cli->fd.rfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    if (cli->fd.rfd < 0) return -1;

    int opt = 1;
    if (0 > setsockopt(cli->fd.rfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
        perror("setsockopt");
        return -1;
    }
    if (0 > setsockopt(cli->fd.rfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))){
        perror("setsockopt");
        return -1;
    }

    return 0;
}

void ln_usock_close(ln_usocket *cli){
    if (!cli) return;
    close(cli->fd.rfd);
}

int ln_usock_bind(ln_usocket *cli, naddr_t addr){
    ln_netfd(addr, &cli->fd);

    return bind(
        cli->fd.rfd,
        (const struct sockaddr*)&cli->fd.addr,
        cli->fd.addr_len
    );
}

ssize_t ln_usock_recv(ln_usocket *cli, void *buf, size_t n, nnet_fd *from){
    if (!cli) return -1;
    
    if (from) {
        from->addr_len = sizeof(struct sockaddr_storage);
        memset(&from->addr, 0, sizeof(from->addr));
    }

    return recvfrom(
        cli->fd.rfd,
        buf, n, 0,
        (from ? (struct sockaddr*)&from->addr: NULL),
        (from ? &from->addr_len: NULL)
    );
}

ssize_t ln_usock_send(ln_usocket *cli, const void *buf, size_t n, nnet_fd to){
    if (!cli) return -1;
    
    return sendto(
        cli->fd.rfd,
        buf, n, 0,
        (const struct sockaddr*)&to.addr,
        to.addr_len
    );
}
