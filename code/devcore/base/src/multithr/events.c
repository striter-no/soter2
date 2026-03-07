#include <multithr/events.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int mt_evsock_new(mt_eventsock *evsock){
    int fds[2];
    if (0 > socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds)){
        perror("socketpair");
        return -1;
    }
    evsock->client_fd = fds[0];
    evsock->parent_fd = fds[1];
    evsock->pfd = (struct pollfd){.fd = evsock->client_fd, .events = POLLIN};
    return 0;
}

int mt_evsock_close(mt_eventsock *evsock){
    if (!evsock) return -1;
    close(evsock->client_fd);
    close(evsock->parent_fd);
    return 0;
}


int mt_evsock_notify(mt_eventsock *evsock){
    if (!evsock) return -1;
    write(evsock->client_fd, &(char){1}, 1);
    return 0;
}

int mt_evsock_wait(mt_eventsock *evsock, int timeout){
    if (!evsock) return -1;
    
    int r = poll(&evsock->pfd, 1, timeout);
    if (0 >= r) return r;

    read(evsock->client_fd, &(char){1}, 1);
    return r;
}
