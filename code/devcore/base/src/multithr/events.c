#include <errno.h>
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
    
    char dummy = 1;
    if (write(evsock->parent_fd, &dummy, 1) < 0) {
        return -1;
    }
    return 0;
}

int mt_evsock_wait(mt_eventsock *evsock, int timeout){
    if (!evsock) return -1;
    
    int r = poll(&evsock->pfd, 1, timeout);
    if (r <= 0) return r;

    if (evsock->pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        return -1; 
    }

    if (evsock->pfd.revents & POLLIN) {
        char buffer[256];
        ssize_t bytes_read;
        
        while ((bytes_read = read(evsock->client_fd, buffer, sizeof(buffer))) > 0) {
            // draining
        }
        
        if (bytes_read == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("error in mt_evsock_wait");
                return -1;
            }
        }
        
        if (bytes_read == 0) {
            return -1;
        }
    }

    return r;
}

int mt_evsock_drain(mt_eventsock *evsock){
    if (!evsock) return -1;
    char buffer[256];
    ssize_t bytes_read;
    
    while ((bytes_read = read(evsock->client_fd, buffer, sizeof(buffer))) > 0) {
        // drain
    }
    
    // Игнорируем EAGAIN, но сообщаем о реальных ошибках
    if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("error in mt_evsock_drain");
        return -1;
    }
    
    return 0;
}