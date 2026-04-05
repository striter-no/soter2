#include <errno.h>
#include <multithr/events.h>
#include <multithr/time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/eventfd.h>

int mt_evsock_new  (mt_eventsock *evsock){
    if (!evsock) return -1;

    evsock->_write_fd = -1;
    evsock->event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

    evsock->pfd = (struct pollfd){evsock->event_fd, POLLIN, 0};
    return 0;
}
int mt_evsock_close(mt_eventsock *evsock){
    if (!evsock) return -1;

    close(evsock->event_fd);
    return 0;
}

int mt_evsock_notify(mt_eventsock *evsock){
    if (!evsock) return -1;

    write(evsock->event_fd, &(uint64_t){1}, sizeof(uint64_t));
    return 0;
}

int mt_evsock_wait  (mt_eventsock *evsock, int timeout){
    if (!evsock) return -1;

    int r = poll(&evsock->pfd, 1, timeout);
    if (r <= 0) {
        if (r < 0) perror("poll error in mt_evsock_wait");
        return r;
    }

    if (evsock->pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        fprintf(stderr, "[mt_evsock_wait] poll revents: 0x%x\n", evsock->pfd.revents);
        return -1;
    }

    if (evsock->pfd.revents & POLLIN) {
        uint64_t b;
        read(evsock->event_fd, &b, sizeof(b));

        return r;
    }

    return 0;
}

int mt_evsock_waitm (mt_eventsock *evsocks, int n, int timeout){
    struct pollfd *fds = malloc(sizeof(*fds) * n);
    for (int i = 0; i < n; i++){
        fds[i] = evsocks[i].pfd;
    }

    int r = poll(fds, n, timeout);
    if (r <= 0) {
        if (r < 0) perror("poll error in mt_evsock_wait");

        free(fds);
        return r;
    }

    for (int i = 0; i < n; i++){
        if (evsocks[i].pfd.revents & POLLIN) {
            uint64_t b;
            read(evsocks[i].event_fd, &b, sizeof(b));
        }
    }

    free(fds);
    return r;
}

int mt_evsock_drain (mt_eventsock *evsock){
    if (!evsock) return -1;

    uint64_t b;
    read(evsock->event_fd, &b, sizeof(b));
    return 0;
}

#else

int mt_evsock_new(mt_eventsock *evsock){
    int fds[2];
    if (0 > socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds)){
        perror("socketpair");
        return -1;
    }
    evsock->event_fd = fds[0];
    evsock->_write_fd = fds[1];
    evsock->pfd = (struct pollfd){.fd = evsock->event_fd, .events = POLLIN};
    return 0;
}

int mt_evsock_close(mt_eventsock *evsock){
    if (!evsock) return -1;
    close(evsock->event_fd);
    close(evsock->_write_fd);
    return 0;
}

int mt_evsock_notify(mt_eventsock *evsock){
    if (!evsock) return -1;

    char dummy = 1;
    if (write(evsock->_write_fd, &dummy, 1) < 0) {
        return -1;
    }
    return 0;
}

int mt_evsock_wait(mt_eventsock *evsock, int timeout){
    if (!evsock) return -1;

    int r = poll(&evsock->pfd, 1, timeout);
    if (r <= 0) {
        if (r < 0) perror("poll error in mt_evsock_wait");
        return r;
    }

    if (evsock->pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        fprintf(stderr, "[mt_evsock_wait] poll revents: 0x%x\n", evsock->pfd.revents);
        return -1;
    }

    if (evsock->pfd.revents & POLLIN) {
        char buffer[256];
        ssize_t bytes_read;

        while ((bytes_read = read(evsock->event_fd, buffer, sizeof(buffer))) > 0) {}

        if (bytes_read == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("read error in mt_evsock_wait");
                return -1;
            }
        }

        if (bytes_read == 0) {
            fprintf(stderr, "[mt_evsock_wait] EOF on event socket\n");
            return -1;
        }
    }

    return r;
}

int mt_evsock_drain(mt_eventsock *evsock){
    if (!evsock) return -1;
    char buffer[256];
    ssize_t bytes_read;

    while ((bytes_read = read(evsock->event_fd, buffer, sizeof(buffer))) > 0) {}

    if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("error in mt_evsock_drain");
        return -1;
    }

    return 0;
}

#endif
