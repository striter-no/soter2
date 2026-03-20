#include <soter2/interface.h>
#include <errno.h>
#include <unistd.h>

static void e2ee_transport(soter2_interface *intr, e2ee_connection *conn);
int main(void){
    srand(mt_time_get_seconds_monocoarse());

    soter2_interface intr;
    if (0 > soter2_intr_init(&intr)) {
        fprintf(stderr, "[main] failed to init interface\n");
        return -1;
    }

    stun_addr addresses[] = {
        {"stun.sipnet.ru", 3478},
        {"stun.ekiga.net", 3478}
    };

    nat_type nt = soter2_intr_STUN(&intr,  addresses, sizeof(addresses) / sizeof(addresses[0]));

    printf("Current NAT type: %s\n", strnattype(nt));
    printf("My address: %s:%u:%u\n", intr.sock.addr.ip.v4.ip, intr.sock.addr.ip.v4.port, intr.rudp_disp.self_uid);

    soter2_intr_stateconn(&intr, ln_uniq("127.0.0.1", 9000), 2);

    if (0 > soter2_intr_run(&intr)){
        fprintf(stderr, "[main] failed to run interface\n");
        return -1;
    }

    state_request req;
    printf("[main] waiting for new client\n");
    soter2_intr_wait_state(&intr, -1, &req);
    printf("[main] got new client\n");

    peer_info info;
    rudp_connection *conn = NULL;
    soter2_iconnect(&intr, ln_from_uint32(req.ip, req.port), req.uid);
    soter2_istatewait(&intr, req.uid, PEER_ST_ACTIVE, &info);
    soter2_intr_statestop(&intr);
    soter2_inew_conn(&intr, &conn, &info.nfd, req.uid);

    printf("[main] e2ee wrapping...\n");
    e2ee_connection econn;
    soter2_e2ee_wrap(&intr, conn, &econn, req.pubkey);
    soter2_e2ee_handshake(&econn);
    soter2_e2ee_end_handshake(&econn, -1);

    // raw_transport(&intr, conn);
    e2ee_transport(&intr, &econn);

    rudp_close_conncetion(&intr.rudp_disp, info.UID);
    free(conn);

    soter2_intr_end(&intr);
}

static void e2ee_transport(soter2_interface *intr, e2ee_connection *conn){
    int i = 0;
    while (!conn->conn->closed){
        char data[200]; snprintf(data, 50, "Hello %d", i);

        e2ee_send(conn, data, strlen(data));
        
        int w = e2ee_wait(conn, conn->conn->ack_timeout_ms);
        if (w == 0) {
            usleep(10000);
            continue;
        } else if (w < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            perror("[main][loop] iwait");
            break;
        }

        protopack *r;
        while ((r = e2ee_recv(conn)) != NULL) {
            if (i % 100 == 0)
                printf("> %.*s  (gseq: %u, dps: %f)\n", r->d_size, r->data, r->seq, soter2_get_DPS(intr));
            free(r);
            i++;
        }
    }
}