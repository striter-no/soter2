
#include "rudp/_modules.h"
#define SOTER_DEAD_DT 10

#include <soter2/interface.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

void *sender_thr(void *_args){ 
    rudp_connection *conn = _args;
    for (int i = 0; i < 1000;){
        char data[200]; snprintf(data, 50, "Hello %d", i);

        prot_array_lock(&conn->pkts_fhost);
        int in_flight = conn->pkts_fhost.array.len;
        prot_array_unlock(&conn->pkts_fhost);

        if (in_flight < 16) { // Окно отправки (например, 16)
            soter2_isend(conn, data, strlen(data));
            i++;
        } else {
            printf("[loop_sender] in_flight > 16: %d\n", in_flight);
            usleep(10000); // Ждем подтверждения наших пакетов
        }

        // printf("[loop_sender] avg_rtt_ms: %li\n", conn->avg_rtt_ms);
        // usleep(conn->avg_rtt_ms * 1000);
    }

    printf("sender_end");
    return NULL;
}

void *reader_thr(void *_args){ 
    rudp_connection *conn = _args;
    for (int i = 0; i < 1000;){
        int w = rudp_conn_wait(conn, conn->ack_timeout_ms);
        if (w == 0) {
            printf("[main][loop] skiped %d iter\n", i);
            usleep(10000);
            continue;
        } else if (w < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            perror("[main][loop] iwait");
            break;
        }

        protopack *r;
        while ((r = soter2_irecv(conn)) != NULL) {
            printf("> %.*s  (gseq: %u)\n", r->d_size, r->data, r->seq);
            free(r);
            i++;
        }
    }

    printf("reader_end");

    return NULL;
}

int main(){
    srand(mt_time_get_seconds_monocoarse());

    soter2_interface intr;
    if (0 > soter2_intr_init(&intr)) {
        fprintf(stderr, "[main] failed to init interface\n");
        return -1;
    }

    // soter2_intr_upd_sign(&intr, "dev/.sign_soter2");
    
    nat_type nt = soter2_intr_STUN(
        &intr, 
        ln_resolveq("stun.sipnet.ru", 3478),
        ln_resolveq("stun.ekiga.net", 3478)
    );

    printf("Current NAT type: %s\n", strnattype(nt));
    printf("My address: %s:%u:%u\n", intr.sock.addr.ip.v4.ip, intr.sock.addr.ip.v4.port, intr.rudp_disp.self_uid);

    soter2_intr_stateconn(&intr, ln_make4(ln_ipv4("127.0.0.1", 9000)), 2);

    if (0 > soter2_intr_run(&intr)){
        fprintf(stderr, "[main] failed to run interface\n");
        return -1;
    }

    // state_request req;
    // printf("[main] waiting for new client\n");
    // soter2_intr_wait_state(&intr, -1, &req);
    // printf("[main] got new client\n");

    char ip[INET_ADDRSTRLEN]; 
    unsigned port, uid;
    printf("[main] ip:port:uid > "); fflush(stdin);
    scanf("%[^:]:%u:%u", ip, &port, &uid);

    peer_info info;
    rudp_connection *conn = NULL;
    soter2_iconnect(&intr, ln_make4(ln_ipv4(ip, port)), uid);
    // soter2_iconnect(&intr, ln_from_uint32(req.ip, req.port), req.uid);
    soter2_istatewait(&intr, uid, PEER_ST_ACTIVE, &info);
    soter2_intr_statestop(&intr);
    // soter2_inew_conn(&intr, &conn, &info.nfd, req.uid);
    soter2_inew_conn(&intr, &conn, &info.nfd, uid);

    // e2ee_connection econn;
    // if (0 > soter2_e2ee_wrap(&intr, conn, &econn, req.pubkey)){
    //     fprintf(stderr, "[main][e2ee] failed to wrap channel\n");
    //     goto end;
    // }

    // printf("[main][e2ee] handshaking...\n");
    // if (0 > e2ee_conn_handshake_init(&econn)){
    //     fprintf(stderr, "[main][e2ee] hs init failed\n");
    //     goto end;
    // }
    // if (0 >= e2ee_conn_handshake_wait(&econn, -1)){
    //     fprintf(stderr, "[main][e2ee] hs wait failed\n");
    //     goto end;
    // }
    // if (0 > e2ee_conn_handshake_resp(&econn)){
    //     fprintf(stderr, "[main][e2ee] hs response failed\n");
    //     goto end;
    // }

    // printf("[main][e2ee] handshaked!\n");

    pthread_t t[2] = {0};
    pthread_create(&t[0], NULL, sender_thr, conn);
    pthread_create(&t[1], NULL, reader_thr, conn);
    sleep(100000);

    pthread_join(t[0], NULL);
    pthread_join(t[1], NULL);

    rudp_conn_close(conn);

    // flushing incoming packets
    // if (0 < e2ee_wait(&econn, 2000)){
    //     protopack *r = e2ee_recv(&econn);
    //     printf("> %.*s\n", r->d_size, r->data);
    //     free(r);
    // }

    soter2_intr_end(&intr);
}