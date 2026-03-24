#include "onion/packets.h"
#include "rudp/_modules.h"
#include <onion/circut.h>

// -- circut lifecycle
int rele_circut_new  (rele_circut *circut, uint32_t circut_uid){
    if (!circut) return -1;

    circut->peers       = NULL;
    circut->reles_count = 0;
    circut->circut_uid  = circut_uid;

    return 0;
}

int rele_circut_clear(rele_circut *circut){
    if (!circut) return -1;

    free(circut->peers);
    circut->peers = NULL;

    circut->reles_count = 0;
    return 0;
}

// -- circut client
int rele_circut_extend(rele_circut *circut, peer_info *info){
    if (!circut || !info) return -1;

    circut_peer *peers = realloc(circut->peers, (circut->reles_count + 1) * sizeof(circut_peer));
    if (!peers) return -1;

    circut->peers = peers;
    circut_peer *peer = &circut->peers[circut->reles_count];
    *peer = (circut_peer){ .info = *info };
    circut->reles_count++;

    return 0;
}

int rele_circut_send_r(rele_circut *circut, size_t curr_peer, void *data, size_t d_size, onion_cmd cmd){
    if (!circut) return -1;
    if (curr_peer >= circut->reles_count) return -1;

    onion_packet pkt;
    if (0 > onion_pack_construct(&pkt, data, d_size, circut->circut_uid, cmd))
        return -1;

    rele_circut_send(circut, curr_peer, &pkt);
    return 0;
}

int rele_circut_send(rele_circut *circut, size_t curr_peer, onion_packet *pkt){
    if (!circut || !pkt) return -1;
    if (curr_peer >= circut->reles_count) return -1;

    unsigned char buf[sizeof(*pkt)];
    if (0 > onion_pack_serial(pkt, buf))
        return -1;

    e2ee_send(circut->peers[curr_peer].e2ee_conn, buf, sizeof(*pkt));
    return 0;
}

int rele_circut_wait(rele_circut *circut, size_t curr_peer, int timeout){
    if (!circut) return -1;
    if (curr_peer >= circut->reles_count) return -1;

    int r = mt_evsock_wait(&circut->inc_ev, timeout);
    if (r <= 0) return r;

    size_t i = 0;
    while (true){
        ; // ???
    }

    return -1;
}

int rele_circut_wait_any(rele_circut *circut, int timeout){
    if (!circut || circut->reles_count == 0) return -1;

    return mt_evsock_wait(&circut->inc_ev, timeout);
}

int _rele_circut_pass(rele_circut *circut, size_t curr_peer, onion_packet *pkt){
    if (!circut || !pkt) return -1;
    if (curr_peer >= circut->reles_count) return -1;

    if (0 > prot_queue_push(&circut->inc_packets, pkt))
        return -1;

    mt_evsock_notify(&circut->inc_ev);
    return 0;
}
