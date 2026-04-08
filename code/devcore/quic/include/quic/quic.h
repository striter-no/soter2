/*
 * quic:quic
 * module for wrapping QUIC library for simplicity
 */

#ifndef QUIC_HEADER_H
#define QUIC_HEADER_H

#define QUIC_PROTO "SOTER2_QUIC_PROTO"
#define QUIC_MTU    1500

#include <stdatomic.h>
#include <picoquic.h>
#include <picoquic_utils.h>
#include <lownet/core.h>
#include <base/prot_queue.h>
#include <base/prot_table.h>
#include <multithr/events.h>
#include <multithr/time.h>

typedef struct {
    uint64_t stream_id;
    uint8_t *msg;
    size_t   msg_len;
    bool     fin;
} quic_pkt;

typedef struct {
    picoquic_cnx_t  *cnx;
    prot_queue       inc_pkts;
} quic_session;

typedef struct {
    atomic_bool is_running;
    ln_socket  *ptr_sock;
    size_t      mtu;

    mt_eventsock incoming_ev;
    mt_eventsock outgoing_ev;
    mt_eventsock outgoing_done_ev;

    uint64_t delay_us;
    picoquic_quic_t *ctx;
    picoquic_cnx_t  *cnx;

    prot_array   sessions;
    mt_eventsock new_session_ev;
    prot_queue   new_sessions;
} quic_core;

int quic_serv_init(quic_core *core, const char *path_to_certificate, const char *path_to_private_key, ln_socket *ptr_sock);
int quic_cli_init (quic_core *core, ln_socket *ptr_sock);
int quic_cli_start(quic_core *core);

int quic_core_stop(quic_core *core);
int quic_core_clear(quic_core *core);
bool quic_core_running(quic_core *core);

int quic_wait_session(quic_core *core, quic_session **session, int timeout);
int quic_wait_recviter(quic_core *core);
int quic_wait_incpkt(quic_core *core, int timeout);

int quic_core_senditer(quic_core *core);
int quic_core_recviter(quic_core *core);

int quic_send(quic_core *core, const quic_session *session, const quic_pkt *pkt);
int quic_recv(quic_session *session, quic_pkt *pkt);
int quic_core_wait_done(quic_core *core, int timeout);

quic_pkt quic_packet(uint64_t stream_id, uint8_t *msg, size_t msg_len, bool fin);
void     quic_packet_free(quic_pkt *pkt);

#endif
