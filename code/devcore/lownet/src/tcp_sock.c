#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <lownet/tcp_sock.h>
#include <stdio.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <multithr/time.h>

int ln_tsock_new(ln_socket *sck){
    if (!sck) return -1;

    sck->fd.rfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
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

void ln_tsock_close(ln_socket *sck){
    if (!sck) return;

    close(sck->fd.rfd);
}

int ln_tsock_connect(ln_socket *sck, naddr_t addr){
    if (!sck) return -1;

    sck->addr = addr;
    ln_netfd(&addr, &sck->fd);

    return connect(
        sck->fd.rfd,
        (struct sockaddr*)&sck->fd.addr,
        sck->fd.addr_len
    );
}

int ln_tsock_connectx(ln_socket *sck, naddr_t addr, int timeout){
    int r = ln_tsock_connect(sck, addr);

    if (r < 0 && (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS)){
        int tr = ln_wait_netfd(&sck->fd, POLLOUT, timeout);
        if (tr <= 0) return tr;

        int so_error = 0;
        socklen_t len = sizeof(so_error);
        if (getsockopt(sck->fd.rfd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0 || so_error != 0) {
            errno = so_error;
            return -1;
        }

        return 1;
    } else if (r < 0) {
        return r;
    }

    return 1;
}

int ln_tsock_bind(ln_socket *sck, naddr_t addr){
    if (!sck) return -1;

    sck->addr = addr;
    ln_netfd(&addr, &sck->fd);

    return bind(
        sck->fd.rfd,
        (struct sockaddr*)&sck->fd.addr,
        sck->fd.addr_len
    );
}


int ln_tsock_listen(ln_socket *sck, int n){
    if (!sck) return -1;
    return listen(sck->fd.rfd, n);
}

int ln_tsock_accept4(ln_socket *sck, ln_socket *new_sock){
    if (!sck || !new_sock) return -1;

    int new_fd = accept4(
        sck->fd.rfd,
        (struct sockaddr*)&new_sock->fd.addr,
        &new_sock->fd.addr_len,
        SOCK_NONBLOCK | SOCK_CLOEXEC
    );

    if (new_fd < 0)
        return -1;

    new_sock->addr = ln_nfd2addr(&new_sock->fd);
    new_sock->fd.rfd = new_fd;
    return 0;
}


ssize_t ln_tsock_read(ln_socket *sck, void *buf, size_t n){
    if (!sck || !buf) return -1;

    return read(sck->fd.rfd, buf, n);
}

ssize_t ln_tsock_write(ln_socket *sck, void *buf, size_t n){
    if (!sck || !buf) return -1;

    return write(sck->fd.rfd, buf, n);
}

ssize_t ln_tsock_readx (ln_socket *sck, size_t n, void *buf, int timeout_ms){
    if (!sck || !buf) return -1;

    size_t already_read = 0;
    int start_ms = mt_time_get_millis_monocoarse();
    while (true) {

        ssize_t rr = ln_tsock_read(sck, (unsigned char*)buf + already_read, n - already_read);
        if (rr == -1 && (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)){
            if (errno == EINTR) continue;

            int pr = ln_wait_netfd(&sck->fd, POLLIN, timeout_ms);
            if (pr > 0) continue;
            if (pr == -1) return -1;
            if (pr == 0) return already_read;

            return -1;
        } else if (rr == 0){
            return already_read;
        }

        already_read += rr;

        if (already_read >= n) break;
        if (timeout_ms == -1) continue;

        int curr_ms = mt_time_get_millis_monocoarse();
        if (curr_ms - start_ms >= timeout_ms) break;
    }

    return already_read;
}

ssize_t ln_tsock_writex(ln_socket *sck, size_t n, void *buf, int timeout_ms){
    if (!sck || !buf) return -1;

    size_t already_written = 0;
    int start_ms = mt_time_get_millis_monocoarse();
    while (true) {

        ssize_t rw = ln_tsock_write(sck, (unsigned char*)buf + already_written, n - already_written);
        if (rw == -1){
            if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
                continue;

            int pr = ln_wait_netfd(&sck->fd, POLLOUT, timeout_ms);
            if (pr > 0) continue;
            if (pr == -1) return -1;
            if (pr == 0) return already_written;

            return -1;
        } else if (rw == 0){
            return already_written;
        }

        already_written += rw;

        if (already_written >= n) break;
        if (timeout_ms == -1) continue;

        int curr_ms = mt_time_get_millis_monocoarse();
        if (curr_ms - start_ms >= timeout_ms) break;
    }

    return already_written;
}
