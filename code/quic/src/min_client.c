#include <assert.h>
#include <quic/quic.h>
#include <lownet/udp_sock.h>
#include <multithr/daemons.h>

bool serv_daemon(void *_args){
    quic_core *core = _args;

    while (quic_core_running(core)){
        quic_core_senditer(core);
        if (quic_wait_recviter(core) > 0) {
            quic_core_recviter(core);
        }
    }

    return false;
}

int main(void) {
    ln_socket sock;
    ln_usock_new(&sock);
    ln_uni("127.0.0.1", 4433, &sock.addr);
    ln_netfd(&sock.addr, &sock.fd);

    quic_core core;
    quic_cli_init(&core, &sock);
    quic_cli_start(&core);

    mdaemon daem;
    daemon_run(&daem, false, serv_daemon, &core);

    quic_session *session = NULL;
    quic_wait_session(&core, &session, -1);
    printf("[+] client connected\n");

    quic_pkt pkt = quic_packet(0, (uint8_t*)"Hello from client", -1, false);
    quic_send(&core, session, &pkt);
    quic_packet_free(&pkt);

    quic_wait_incpkt(&core, -1);
    quic_recv(session, &pkt);
    printf("[+] got pkt: %.*s\n", (int)pkt.msg_len, pkt.msg);
    quic_packet_free(&pkt);

    quic_core_stop(&core);
    daemon_stop(&daem);

    quic_core_clear(&core);
    ln_usock_close(&sock);
}
