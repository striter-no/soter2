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
    printf("My address: %s:%u:%u\n", ln_gip(&intr.sock.addr), ln_gport(&intr.sock.addr), intr.rudp_disp.self_uid);

    soter2_intr_stateconn(&intr, ln_uniq("127.0.0.1", 9000), 1);
    // int st_UID2 = soter2_intr_stateconn(&intr, ln_uniq("192.168.1.5", 9001), 2);

    if (0 > soter2_intr_run(&intr)){
        fprintf(stderr, "[main] failed to run interface\n");
        return -1;
    }

    printf("[main] waiting for client\n");    
    rudp_connection *conn;
    if (0 > soter2_intr_wait_conn(&intr, &conn, -1)){
        fprintf(stderr, "[main] failed to get new connection\n");
        return -1;
    }

    peer_info inf;
    peers_db_get(&intr.pdb, conn->c_uid, &inf);
    printf("[main] got RUDP connection\n");

    // soter2_iget_conn(&intr, &conn, UINT32_MAX);

    printf("[main] e2ee wrapping...\n");
    e2ee_connection econn;
    soter2_e2ee_wrap(&intr, conn, &econn, inf.pubkey);
    
    if (0 > soter2_e2ee_handshake(&econn)){
        fprintf(stderr, "[main] failed to send e2ee handshake\n"); 
        return -1;
    }
    
    if (0 > soter2_e2ee_end_handshake(&econn, -1)){
        fprintf(stderr, "[main] failed to end e2ee handshake\n"); 
        return -1;
    }

    // raw_transport(&intr, conn);
    e2ee_transport(&intr, &econn);

    rudp_close_conncetion(&intr.rudp_disp, conn->c_uid);

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