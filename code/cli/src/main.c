
#include "rudp/_modules.h"
#define SOTER_DEAD_DT 10

#include <soter2/interface.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(){
    srand(mt_time_get_seconds());

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

    state_request req;
    printf("[main] waiting for new client\n");
    soter2_intr_wait_state(&intr, -1, &req);
    printf("[main] got new client\n");

    // char ip[INET_ADDRSTRLEN]; 
    // unsigned port, uid;
    // printf("[main] ip:port:uid > "); fflush(stdin);
    // scanf("%[^:]:%u:%u", ip, &port, &uid);

    peer_info info;
    rudp_connection *conn = NULL;
    // soter2_iconnect(&intr, ln_make4(ln_ipv4(ip, port)), uid);
    soter2_iconnect(&intr, ln_from_uint32(req.ip, req.port), req.uid);
    soter2_istatewait(&intr, req.uid, PEER_ST_ACTIVE, &info);
    soter2_intr_statestop(&intr);
    soter2_inew_conn(&intr, &conn, &info.nfd, req.uid);

    e2ee_connection econn;
    if (0 > soter2_e2ee_wrap(&intr, conn, &econn, req.pubkey)){
        fprintf(stderr, "[main][e2ee] failed to wrap channel\n");
        goto end;
    }

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

    for (int i = 0; i < 100000; i++){
        char data[200]; snprintf(data, 50, "Hello %d", i);
        
        while (0 != soter2_isend(conn, data, strlen(data))){
            usleep(10'000'000);
        }

        // while (0 != e2ee_send(&econn, data, strlen(data))){
        //     usleep(10'000'000);
        // }

        // int w = e2ee_wait(&econn, SOTER_DEAD_DT * 1000);
        int w = rudp_conn_wait(conn, SOTER_DEAD_DT * 1000);
        if (w == 0) {
            continue;
        } else if (w < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            perror("[main][loop] iwait");
            break;
        }

        // protopack *r = e2ee_recv(&econn);
        protopack *r = soter2_irecv(conn);
        printf("> %.*s  (dps: %f, gseq: %u)\n", r->d_size, r->data, soter2_get_DPS(&intr), r->seq);
        // free(r);

    }

    // flushing incoming packets
    if (0 < e2ee_wait(&econn, 2000)){
        protopack *r = e2ee_recv(&econn);
        printf("> %.*s\n", r->d_size, r->data);
        free(r);
    }

end:
    soter2_intr_end(&intr);
}