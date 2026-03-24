#include <base/prot_queue.h>
#include <lownet/core.h>
#include <multithr/events.h>

#ifndef TURN_HANDLERS_H
#define TURN_HANDLERS_H

typedef struct {
    uint32_t turn_sUID;

    mt_eventsock outgoing_ev;
    prot_queue   outgoing_tpacks;
} turn_client;

int turn_client_init(turn_client *cli, uint32_t turn_sUID);
int turn_client_end (turn_client *cli);

#endif
