#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <stdint.h>
#include <sys/types.h>
#endif

#include <soter2/modules.h>
#include <soter2/handlers.h>

#ifndef SOTER_INTERFACE_H

#ifndef SOTER_DEAD_DT
#define SOTER_DEAD_DT 6
#endif

#ifndef SOTER_REPING_DT
#define SOTER_REPING_DT 3
#endif

#ifndef SOTER_GOSSIP_DT
#define SOTER_GOSSIP_DT 10
#endif

typedef struct {
    ln_usocket sock;
    nat_type   NAT;

    rudp_dispatcher rudp_disp;
    peers_db        pdb;
    gossip_system   gsyst;

    watcher         wtch;
    pvd_listener    listener;
    pvd_sender      sender;

    app_context     ctx;
    pthread_t       iter_daemon;
    atomic_bool     is_running;
} soter2_interface;

int soter2_intr_init(soter2_interface *intr, uint32_t UID);

void soter2_intr_end (soter2_interface *intr);
int  soter2_intr_run (soter2_interface *intr);

int soter2_iwait_chan(soter2_interface *intr, uint32_t client_uid);
rudp_channel *soter2_inew_chan(soter2_interface *intr, nnet_fd nfd, uint32_t client_uid);
rudp_channel *soter2_iget_chan(soter2_interface *intr, uint32_t client_uid);
int soter2_iwait_pack   (rudp_channel *chan, int timeout);
void soter2_iconnect(soter2_interface *intr, naddr_t address, uint32_t UID);
protopack *soter2_irecv (rudp_channel *chan);
int soter2_isend_r  (soter2_interface *intr, nnet_fd nfd, protopack *p);
int soter2_isend    (soter2_interface *intr, rudp_channel *chan, void *data, size_t dsize);
int soter2_istatewait(soter2_interface *intr, uint32_t client_uid, peer_state state, peer_info *info);

void     soter2_punch(app_context ctx, app_peer_info peer);
nat_type soter2_intr_STUN(soter2_interface *intr, naddr_t stun1, naddr_t stun2);


#endif
#define SOTER_INTERFACE_H