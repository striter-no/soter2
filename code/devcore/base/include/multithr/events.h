#include <sys/socket.h>
#include <poll.h>

#ifndef MULTITHR_EVENTS_H

typedef struct {
    int parent_fd;
    int client_fd;
    struct pollfd pfd;
} mt_eventsock;

int mt_evsock_new  (mt_eventsock *evsock);
int mt_evsock_close(mt_eventsock *evsock);

int mt_evsock_notify(mt_eventsock *evsock);
int mt_evsock_wait  (mt_eventsock *evsock, int timeout);

#endif
#define MULTITHR_EVENTS_H