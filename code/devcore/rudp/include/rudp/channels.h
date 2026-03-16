#include <stdint.h>
#include <lownet/core.h>
#include <base/prot_queue.h>
#include <multithr/events.h>
#include <multithr/time.h>
#include <packproto/proto.h>
#include <providers/sender.h>

#ifndef RUDP_CHANNELS_H

#ifndef MAX_WINDOW_SIZE
#define MAX_WINDOW_SIZE 32
#endif

typedef struct {
    uint32_t next_seq;
    uint32_t last_recved_seq;

    uint32_t last_ack_received;
    uint32_t last_ack_sent;
    prot_array pending_queue; // from user
    prot_queue network_queue; // from net
    prot_array reorder_buffer;
    prot_queue reoredered_queue; // from net, already reordered

    mt_eventsock  reordered_fd; // set, used
    // mt_eventsock  network_fd; // set
    mt_eventsock  pending_fd; // set, used
    nnet_fd       client_nfd; // set
    uint32_t      client_uid;
    uint32_t      self_uid;
} rudp_channel;

int rudp_channel_new(rudp_channel *c, nnet_fd *client_nfd, uint32_t self_uid, uint32_t client_uid);
int rudp_channel_end(rudp_channel *c);
int rudp_channel_wait(rudp_channel *c, int timeout);

int rudp_channel_send(pvd_sender *s, rudp_channel *c, protopack *p, nnet_fd *nfd);
int rudp_channel_recv(rudp_channel *c, protopack **p); 

#endif
#define RUDP_CHANNELS_H