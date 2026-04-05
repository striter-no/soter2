#include <sys/socket.h>
#include <poll.h>

#ifndef MULTITHR_EVENTS_H

typedef struct {
    int _write_fd;
    int event_fd;

    struct pollfd pfd;
} mt_eventsock;

int mt_evsock_new  (mt_eventsock *evsock);
int mt_evsock_close(mt_eventsock *evsock);

int mt_evsock_notify(mt_eventsock *evsock);
int mt_evsock_wait  (mt_eventsock *evsock, int timeout);
int mt_evsock_waitm (mt_eventsock *evsocks, int n, int timeout);
int mt_evsock_drain (mt_eventsock *evscok);

#endif
#define MULTITHR_EVENTS_H
