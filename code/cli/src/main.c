#include "rudp/_modules.h"
#include <soter2/interface.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

peer_info input_pinfo(const char *prompt, peer_relay_state rst){
    printf("%s", prompt); fflush(stdin);

    char ip[INET_ADDRSTRLEN]; 
    unsigned port, uid;

    char pubkey_b64[256];
    unsigned char pubkey[CRYPTO_PUBKEY_BYTES] = {0};
    scanf("%[^:]:%u:%s", ip, &port, pubkey_b64);
    crypto_decode_pubkey_uid(pubkey_b64, pubkey, &uid);

    naddr_t addr = ln_uniq(ip, port);
    peer_info info = {
        .state     = PEER_ST_INITED,
        .UID       = uid,
        .last_seen = mt_time_get_millis_monocoarse(),
        .ctx       = NULL,
        .relay_st  = rst,
        .nfd       = ln_netfdq(&addr)
    };
    memcpy(info.pubkey, pubkey, CRYPTO_PUBKEY_BYTES);
    
    return info;
}

const char* get_selfinfo(soter2_interface *intr){
    static char output[512];
    printf("[self] ip:port   %s:%u\n", ln_gip(&intr->sock.addr), ln_gport(&intr->sock.addr));
    printf("[self] uid       %u\n", intr->rudp_disp.self_uid);
    printf("[self] fingprint %s\n", soter2_fingerprint(intr));

    char *b64 = crypto_encode_pubkey_uid(intr->self_sign.id_pub, intr->rudp_disp.self_uid);
    snprintf(
        output, 512, "%s:%u:%s", 
        ln_gip(&intr->sock.addr), 
        ln_gport(&intr->sock.addr),
        b64
    );

    free(b64);
    return output;
}

static void e2ee_transport(soter2_interface *intr, e2ee_connection *conn);
static void raw_transport(soter2_interface *intr, rudp_connection *conn);
int main(int argc, char *argv[]){
    bool rele_only = false;
    for (int i = 1; i < argc; i++){
        if (0 != strcmp("-rele", argv[i])) continue;

        printf("[main] entering RELE-only mode\n");
        rele_only = true;
    }
    
    if (!rele_only && argc < 3){
        printf("usage: %s PATH STATE ...\n\nPATH - path to cryptographic signature\nSTATE - ip:port of state server\n", argv[0]);
        return 1;
    }

    srand(mt_time_get_seconds_monocoarse());

    soter2_interface intr;
    if (0 > soter2_intr_init(&intr)) {
        fprintf(stderr, "[main] failed to init interface\n");
        return -1;
    }

    if (0 > soter2_intr_upd_sign(&intr, argv[1])){
        fprintf(stderr, "[main] failed to load/save signature to \"%s\"\n", argv[1]);
        return -1;
    }

    stun_addr addresses[] = {
        {"stun.sipnet.ru", 3478},
        {"stun.ekiga.net", 3478}
    };

    nat_type nt = soter2_intr_STUN(&intr,  addresses, sizeof(addresses) / sizeof(addresses[0]));

    printf("[main] current NAT type: %s\n", strnattype(nt));
    for (int i = 2; i < argc; i++){
        char ip[255]; unsigned short port;
        if (argv[i][0] == '-') continue;

        sscanf(argv[i], "%[^:]:%hu", ip, &port);

        printf("[main][st] adding state server %s:%u...\n", ip, port);
        soter2_intr_stateconn(&intr, ln_uniq(ip, port), 1);
    }

    if (0 > soter2_intr_run(&intr)){
        fprintf(stderr, "[main] failed to run interface\n");
        return -1;
    }

    printf("[main] my info:\n%s\n", get_selfinfo(&intr));

    if (rele_only) {
        printf("[main] staying still due to RELE mode\n");
        while (soter2_irunning(&intr)){
            sleep(2);
        }

        goto end;
    }
    // peer_info info = input_pinfo("[main] input other client's info: ", PEER_RE_RELAYED);
    
    // naddr_t addr = ln_nfd2addr(&info.nfd);
    // printf("[main] got client: %s:%u:%u:%s\n", ln_gip(&addr), ln_gport(&addr), info.UID, crypto_fingerprint(info.pubkey));
    // printf("[main] will try make RELE connection with %u\n", info.UID);

    printf("[main] waiting for any RELE client\n");
    // rudp_connection *network_conn;
    rudp_connection *conn;
    if (0 > soter2_intr_wait_conn(&intr, &conn, -1)){
        fprintf(stderr, "[main] failed to get new connection\n");
        return -1;
    }
    printf("[main] successfully connected to network\n");
    printf("[main] trying to open RELE connection\n");

    // soter2_iconnect(&intr, ln_nfd2addr(&info.nfd), info.UID, info.pubkey, PEER_RE_RELAYED);
    // soter2_istatewait(&intr, info.UID, PEER_ST_ACTIVE, &info);
    
    printf("[main] waiting...\n");
    // if (0 > soter2_intr_wait_connspec(&intr, &conn, info.UID)){
    //     fprintf(stderr, "[main] failed to get RELE connection\n");
    //     return -1;
    // }

    peer_info inf;
    peers_db_get(&intr.pdb, conn->c_uid, &inf);
    printf("[main] got RUDP connection\n");

    printf("[main] e2ee wrapping...\n");
    // e2ee_connection econn;
    // soter2_e2ee_wrap(&intr, conn, &econn, inf.pubkey);
    
    // if (0 > soter2_e2ee_handshake(&econn)){
    //     fprintf(stderr, "[main] failed to send e2ee handshake\n"); 
    //     return -1;
    // }
    
    // if (0 > soter2_e2ee_end_handshake(&econn, -1)){
    //     fprintf(stderr, "[main] failed to end e2ee handshake\n"); 
    //     return -1;
    // }

    // e2ee_transport(&intr, &econn);
    raw_transport(&intr, conn);

    soter2_close_conn(&intr, conn->c_uid);

end:
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
            // if (i % 100 == 0)
            printf("> %.*s  (gseq: %u, dps: %f)\n", r->d_size, r->data, r->seq, soter2_get_DPS(intr));
            free(r);
            i++;
        }
    }
}

static void raw_transport(soter2_interface *intr, rudp_connection *conn){
    // int i = 0;
    for(int i = 0; i < 20;){
    // while (!conn->closed){
        char data[200]; snprintf(data, 50, "Hello %d", i);

        soter2_isend(intr, conn, data, strlen(data));
        
        int w = rudp_conn_wait(conn, conn->ack_timeout_ms);
        if (w == 0) {
            usleep(10000);
            continue;
        } else if (w < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            perror("[main][loop] iwait");
            break;
        }

        protopack *r;
        while ((r = soter2_irecv(conn)) != NULL) {
            // if (i % 100 == 0)
            printf("> %.*s  (gseq: %u, dps: %f)\n", r->d_size, r->data, r->seq, soter2_get_DPS(intr));
            free(r);
            i++;
        }
    }
}