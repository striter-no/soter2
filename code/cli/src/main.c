#include <soter2/interface.h>
#include <errno.h>
#include <unistd.h>

int main(){
    srand(mt_time_get_seconds_monocoarse());

    soter2_interface intr;
    if (0 > soter2_intr_init(&intr)) {
        fprintf(stderr, "[main] failed to init interface\n");
        return -1;
    }

    nat_type nt = soter2_intr_STUN(
        &intr, 
        ln_resolveq("stun.sipnet.ru", 3478),
        ln_resolveq("stun.ekiga.net", 3478)
    );

    printf("Current NAT type: %s\n", strnattype(nt));
    printf("My address: %s:%u:%u\n", intr.sock.addr.ip.v4.ip, intr.sock.addr.ip.v4.port, intr.rudp_disp.self_uid);

    if (0 > soter2_intr_run(&intr)){
        fprintf(stderr, "[main] failed to run interface\n");
        return -1;
    }

    char ip[INET_ADDRSTRLEN]; 
    unsigned port, uid;
    printf("[main] ip:port:uid > "); fflush(stdin);
    scanf("%[^:]:%u:%u", ip, &port, &uid);

    peer_info info;
    rudp_connection *conn = NULL;
    soter2_iconnect(&intr, ln_make4(ln_ipv4(ip, port)), uid);
    soter2_istatewait(&intr, uid, PEER_ST_ACTIVE, &info);
    soter2_intr_statestop(&intr);
    soter2_inew_conn(&intr, &conn, &info.nfd, uid);

    for (int i = 0; i < 500;){
        if (conn->closed) break;
        
        char data[200]; snprintf(data, 50, "Hello %d", i);

        prot_array_lock(&conn->pkts_fhost);
        int in_flight = conn->pkts_fhost.array.len;
        prot_array_unlock(&conn->pkts_fhost);

        if (in_flight < 16) {
            soter2_isend(conn, data, strlen(data));
        } else usleep(10000);

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

    rudp_close_conncetion(&intr.rudp_disp, info.UID);
    free(conn);

    soter2_intr_end(&intr);
}