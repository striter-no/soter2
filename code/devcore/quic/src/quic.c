#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <quic/quic.h>
#include <stdint.h>

static int _quic_core_cb(picoquic_cnx_t* cnx, uint64_t stream_id, uint8_t* bytes, size_t length,
                         picoquic_call_back_event_t fin_or_event, void* ctx, void* stream_ctx);

// done
int quic_serv_init(quic_core *core, const char *path_to_certificate, const char *path_to_private_key, ln_socket *ptr_sock){
    if (!core || !path_to_certificate || !path_to_private_key || !ptr_sock) return -1;

    if (0 > mt_evsock_new(&core->incoming_ev)) return -1;
    if (0 > mt_evsock_new(&core->new_session_ev)) return -1;
    if (0 > mt_evsock_new(&core->outgoing_ev)) return -1;
    if (0 > mt_evsock_new(&core->outgoing_done_ev)) return -1;

    core->ptr_sock = ptr_sock;
    core->ctx = picoquic_create(
        8, path_to_certificate, path_to_private_key, NULL, QUIC_PROTO,
        _quic_core_cb, core, NULL, NULL, NULL,
        mt_time_get_micros_monocoarse(), NULL, NULL, NULL, 0
    );

    if (!core->ctx) return -1;
    picoquic_set_log_level(core->ctx, 2);

    core->mtu = QUIC_MTU;
    core->delay_us = 1000000;

    atomic_store(&core->is_running, true);
    prot_array_create(sizeof(quic_session), &core->sessions);
    prot_queue_create(sizeof(quic_session*), &core->new_sessions);
    return 0;
}

// done
int quic_cli_init (quic_core *core, ln_socket *ptr_sock){
    if (!core) return -1;

    if (0 > mt_evsock_new(&core->incoming_ev)) return -1;
    if (0 > mt_evsock_new(&core->new_session_ev)) return -1;
    if (0 > mt_evsock_new(&core->outgoing_ev)) return -1;

    core->ptr_sock = ptr_sock;
    core->ctx = picoquic_create(
        8, NULL, NULL, NULL, QUIC_PROTO,
        _quic_core_cb, core, NULL, NULL, NULL,
        mt_time_get_micros_monocoarse(), NULL, NULL, NULL, 0
    );

    if (!core->ctx) return -1;
    picoquic_set_log_level(core->ctx, 2);

    core->mtu = QUIC_MTU;
    core->delay_us = 1000000;

    atomic_store(&core->is_running, true);
    prot_array_create(sizeof(quic_session), &core->sessions);
    prot_queue_create(sizeof(quic_session*), &core->new_sessions);
    return 0;
}

// done
int quic_cli_start(quic_core *core) {
    uint64_t now = mt_time_get_micros_monocoarse();
    core->cnx = picoquic_create_cnx(core->ctx, picoquic_null_connection_id,
                                             picoquic_null_connection_id, (struct sockaddr*)&core->ptr_sock->fd.addr, now,
                                             0, NULL, QUIC_PROTO, 1);
    if (!core->cnx) return -1;
    return picoquic_start_client_cnx(core->cnx);
}

// done
int quic_core_stop(quic_core *core){
    if (!core) return -1;

    atomic_store(&core->is_running, false);
    return 0;
}

// done
int quic_core_clear(quic_core *core){
    if (!core) return -1;

    for (size_t i = 0; i < prot_array_len(&core->sessions); i++){
        quic_session *ses = prot_array_at(&core->sessions, i);

        quic_pkt pkt;
        while (0 == prot_queue_pop(&ses->inc_pkts, &pkt)){
            quic_packet_free(&pkt);
        }

        prot_queue_end(&ses->inc_pkts);
    }

    prot_queue_end(&core->new_sessions);
    prot_array_end(&core->sessions);
    mt_evsock_close(&core->outgoing_ev);
    mt_evsock_close(&core->incoming_ev);
    mt_evsock_close(&core->new_session_ev);
    mt_evsock_close(&core->outgoing_done_ev);
    picoquic_free(core->ctx);
    return 0;
}

// done
bool quic_core_running(quic_core *core){
    return atomic_load(&core->is_running);
}

// done
int quic_wait_recviter(quic_core *core) {
    if (!core || !core->ptr_sock) return -1;

    struct pollfd fds[] = {
        {.fd = core->ptr_sock->fd.rfd, .events = POLLIN},
        {.fd = core->outgoing_ev.event_fd, .events = POLLIN}
    };

    int timeout_ms = (int)(core->delay_us / 1000);
    if (core->delay_us > 0 && timeout_ms == 0)
        timeout_ms = 1;

    int ret = poll(fds, sizeof(fds) / sizeof(fds[0]), timeout_ms);

    if (ret <= 0) return ret;

    if (fds[1].revents & POLLIN){
        mt_evsock_drain(&core->outgoing_ev);
    }

    if (fds[0].revents & (POLLIN | POLLERR | POLLHUP))
        return 1;

    return 0;
}

// done
int quic_core_senditer(quic_core *core){
    if (!core) return -1;
    uint64_t now = mt_time_get_micros_monocoarse();

    struct sockaddr_storage dest_addr = {0};
    struct sockaddr_storage ret_local_addr = {0};
    int if_index = 0;
    picoquic_connection_id_t log_cid = {0};
    picoquic_cnx_t* last_cnx = NULL;
    size_t send_len = 0;

    mt_evsock_drain(&core->outgoing_done_ev);
    uint8_t packet_buf[PICOQUIC_MAX_PACKET_SIZE] = {0};
    while (picoquic_prepare_next_packet(core->ctx, now, packet_buf, sizeof(packet_buf),
                                        &send_len, &dest_addr, &ret_local_addr,
                                        &if_index, &log_cid, &last_cnx) == 0 && send_len > 0) {

        socklen_t dest_len = (dest_addr.ss_family == AF_INET6) ?
                              sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

        sendto(core->ptr_sock->fd.rfd, packet_buf, send_len, 0, (struct sockaddr*)&dest_addr, dest_len);
    }
    mt_evsock_notify(&core->outgoing_done_ev);

    uint64_t next_wake = picoquic_get_next_wake_time(core->ctx, now);
    core->delay_us = (next_wake > now) ? (next_wake - now): 0;

    if (core->delay_us > 1000000) core->delay_us = 1000000; // 1 sec
    return 0;
}

int quic_core_recviter(quic_core *core) {
    if (!core) return -1;

    struct sockaddr_storage from_addr;
    socklen_t addr_len;
    uint8_t buf[PICOQUIC_MAX_PACKET_SIZE] = {0};

    while (true) {
        addr_len = sizeof(from_addr);
        ssize_t recv_len = recvfrom(core->ptr_sock->fd.rfd, buf, sizeof(buf), 0,
                          (struct sockaddr*)&from_addr, &addr_len);

        if (recv_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            return -1;
        }

        uint64_t now = mt_time_get_micros_monocoarse();
        picoquic_incoming_packet(core->ctx, buf, (size_t)recv_len,
                                (struct sockaddr*)&from_addr,
                                (struct sockaddr*)&core->ptr_sock->fd.addr,
                                0, 0, now);
    }
    return 0;
}

// done
int quic_wait_incpkt(quic_core *core, int timeout){
    if (!core) return -1;
    int r = mt_evsock_wait(&core->incoming_ev, timeout);
    if (r <= 0) return r;
    mt_evsock_drain(&core->incoming_ev);

    return r;
}

int quic_send(quic_core *core, const quic_session *session, const quic_pkt *pkt) {
    if (!core || !session || !pkt) return -1;
    // Используем stream_id из пакета
    int ret = picoquic_add_to_stream(session->cnx, pkt->stream_id, pkt->msg, pkt->msg_len, pkt->fin);
    if (ret == 0) mt_evsock_notify(&core->outgoing_ev);
    if (ret < 0) fprintf(stderr, "[quic] failed to add to stream packet\n");
    return ret;
}

int quic_recv(quic_session *session, quic_pkt *pkt){
    if (!session) return -1;
    // Читаем из очереди конкретной сессии
    return prot_queue_pop(&session->inc_pkts, pkt);
}

// Исправленный двойной указатель
int quic_wait_session(quic_core *core, quic_session **session, int timeout){
    if (!core) return -1;

    if (prot_queue_len(&core->new_sessions) == 0){
        int r = mt_evsock_wait(&core->new_session_ev, timeout);
        if (r <= 0) return r;
    }

    prot_queue_pop(&core->new_sessions, session);
    return 1;
}

quic_pkt quic_packet(uint64_t stream_id, uint8_t *msg, size_t msg_len, bool fin){
    if (msg_len == SIZE_MAX){
        msg_len = strlen((char*)msg);
    }
    quic_pkt pkt = {
        .stream_id = stream_id,
        .msg = malloc(msg_len),
        .msg_len = msg_len,
        .fin = fin
    };
    memcpy(pkt.msg, msg, msg_len);
    return pkt;
}

void quic_packet_free(quic_pkt *pkt){
    if (!pkt || !pkt->msg) return;
    free(pkt->msg);
    pkt->msg = NULL;
}

int quic_core_wait_done(quic_core *core, int timeout){
    if (!core) return -1;
    if (timeout == 0) return 0;

    sleep(1); // draining period
    return mt_evsock_wait(&core->outgoing_done_ev, timeout);
}

static quic_session* _find_session_by_cnx(quic_core *core, picoquic_cnx_t* cnx) {
    prot_array_lock(&core->sessions);
    quic_session *found = NULL;
    for (size_t i = 0; i < core->sessions.array.len; i++) {
        quic_session *ses = _prot_array_at_unsafe(&core->sessions, i);
        if (ses && ses->cnx == cnx) {
            found = ses;
            break;
        }
    }
    prot_array_unlock(&core->sessions);
    return found;
}

static void _quic_register_session(quic_core *core, picoquic_cnx_t* cnx) {
    quic_session session = {
        .cnx = cnx
    };
    prot_queue_create(sizeof(quic_pkt), &session.inc_pkts);

    prot_array_lock(&core->sessions);

    _prot_array_push_unsafe(&core->sessions, &session);
    quic_session *stored_session = _prot_array_at_unsafe(&core->sessions, _prot_array_len_unsafe(&core->sessions) - 1);
    prot_queue_push(&core->new_sessions, &stored_session);
    mt_evsock_notify(&core->new_session_ev);

    prot_array_unlock(&core->sessions);
}

static void _quic_unregister_session(quic_core *core, picoquic_cnx_t* cnx){
    prot_array_lock(&core->sessions);
    for (size_t i = 0; i < core->sessions.array.len; i++){
        quic_session *ses = _prot_array_at_unsafe(&core->sessions, i);
        if (!ses) return;

        if (ses->cnx == cnx) _prot_array_remove_unsafe(&core->sessions, i);
        break;
    }
    prot_array_unlock(&core->sessions);
}

// done
static int _quic_core_cb(picoquic_cnx_t* cnx, uint64_t stream_id, uint8_t* bytes, size_t length,
                         picoquic_call_back_event_t fin_or_event, void* ctx, void* stream_ctx
){
    quic_core *core = ctx;
    switch (fin_or_event) {
        case picoquic_callback_ready: {
            _quic_register_session(core, cnx);
        } break;
        case picoquic_callback_stream_data:
        case picoquic_callback_stream_fin: {
            quic_session *ses = _find_session_by_cnx(core, cnx);
            if (ses) {
                quic_pkt pkt = {
                    .stream_id = stream_id,
                    .msg = malloc(length),
                    .msg_len = length,
                    .fin = fin_or_event == picoquic_callback_stream_fin
                };
                memcpy(pkt.msg, bytes, length);
                prot_queue_push(&ses->inc_pkts, &pkt);
                mt_evsock_notify(&core->incoming_ev);
            } else {
                fprintf(stderr, "[quic] session is not found by %p\n", (void*)cnx);
            }
        } break;
        case picoquic_callback_close:
        case picoquic_callback_stateless_reset:
            _quic_unregister_session(core, cnx);
            quic_core_stop(core);

            break;
        default: break;
    }

    return 0;
}
