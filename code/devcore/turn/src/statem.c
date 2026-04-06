#include <stdint.h>
#include <sys/stat.h>
#include <turn/statem.h>
#include <turn/packets.h>

// done
int turn_client_init(turn_client *cli, uint32_t turn_sUID, pvd_sender  *sender){
    if (!cli || !sender) return -1;

    if (0 > mt_evsock_new(&cli->outgoing_ev)) return -1;
    if (0 > mt_evsock_new(&cli->sys_ev))      return -1;

    // table for pairs
    if (0 > prot_table_create(sizeof(uint32_t), sizeof(turn_bind_peer), DYN_OWN_BOTH, &cli->binded_clients))
        return -1;

    // table for requests
    if (0 > prot_table_create(sizeof(uint32_t), sizeof(turn_sys_status), DYN_OWN_BOTH, &cli->sys_requests))
        return -1;

    cli->turn_sUID = turn_sUID;
    cli->TRUID = rand() % (UINT32_MAX / 2);
    cli->sender = sender;

    return 0;
}

// done
int turn_client_end (turn_client *cli){
    if (!cli) return -1;

    mt_evsock_close(&cli->outgoing_ev);
    mt_evsock_close(&cli->sys_ev);
    prot_table_end(&cli->binded_clients);
    prot_table_end(&cli->sys_requests);

    return 0;
}

// done
int turn_client_bind(turn_client *cli, uint32_t hsh, const turn_bind_peer *peer){
    if (!cli || !peer) return -1;

    prot_table_set(&cli->binded_clients, &hsh, peer);
    return 0;
}

int turn_client_info(turn_client *cli, uint32_t hsh, turn_bind_peer *out_peer){
    if (!cli || !out_peer) return -1;

    void *ptr = prot_table_get(&cli->binded_clients, &hsh);
    if (!ptr) return -1;

    *out_peer = *(turn_bind_peer*)ptr;
    return 0;
}

// done
int turn_client_unbind(turn_client *cli, uint32_t hsh){
    if  (!cli) return -1;

    prot_table_remove(&cli->binded_clients, &hsh);
    return 0;
}

/*
 * Checking hash(from,to), for connectivity check
 * Getting bpeer for address, UID aquired from packet
 *
 * returns pckt[turn[pckt, uid from (ME) -> uid to (PACK->H_TO), linfo]]
 */ // done
int turn_client_wrap(
    turn_client *cli,
    protopack *pack,

    protopack **outgoing
){
    if (!cli || !pack || !outgoing) return -1;

    uint32_t hsh = turn_pair_hash(cli->turn_sUID, pack->h_to);

    // check if user exists
    turn_bind_peer *bpeer = prot_table_get(&cli->binded_clients, &hsh);
    if (!bpeer) return -1;

    char data[2048] = {0};
    size_t totsize = protopack_send(pack, data);

    // if so, wrap packet to outgoing
    turn_request *req = turn_req_make(TURN_DATA_MSG, bpeer->linfo_connected.addr, pack->h_to, 0, data, totsize);
    if (!req) return -1;

    *outgoing = udp_make_pack(
        0, cli->turn_sUID, pack->h_to, PACK_TURN, req, sizeof(*req) + req->d_size
    );

    free(req);
    return 0;
}

// call this method ONLY on TURN_DATA
// done
int turn_client_unwrap(turn_client *cli, protopack *pack, protopack **outgoing){
    if (!cli || !pack || !outgoing) return -1;

    uint32_t hsh = turn_pair_hash(cli->turn_sUID, pack->h_to);

    // check if user exists
    turn_bind_peer *bpeer = prot_table_get(&cli->binded_clients, &hsh);
    if (!bpeer) return -1;

    turn_request *req = turn_req_recv(pack->data, pack->d_size);
    if (!req) return -1;

    *outgoing = protopack_recv((char*)req->data, req->d_size);
    if (!*outgoing) return -1;

    free(req);
    return 0;
}

int turn_client_qsend(turn_client *cli, turn_request *req){
    if (!cli || !req) return -1;

    nnet_fd nfd = ln_netfdq(&req->to_whom);
    protopack *pkt = udp_make_pack(0, cli->turn_sUID, 0, PACK_TURN, req, sizeof(*req));

    pvd_sender_send(cli->sender, pkt, &nfd);
    free(pkt);

    return 0;
}

// create system msg
turn_request* turn_client_req_sys(
    turn_client *cli,
    const naddr_t *to_whom, uint32_t to_UID,
    uint32_t TRUID,
    turn_sys_status status,
    const char *commentary
){
    if (!cli) return NULL;

    // uint32_t TRUID = cli->TRUID++;
    uint8_t data[sizeof(TRUID) + sizeof(status) + (commentary ? strlen(commentary): 0)];
    memset(data, 0, sizeof(data));

    size_t offset = 0;
    memcpy(data + offset, &(uint32_t){ntohl(TRUID)}, sizeof(TRUID));  offset += sizeof(TRUID);
    memcpy(data + offset, &status,                   sizeof(status)); offset += sizeof(status);
    if (commentary){
        memcpy(data + offset, commentary, strlen(commentary)); offset += strlen(commentary);
    }

    return turn_req_make(TURN_SYSTEM_MSG, *to_whom, to_UID, TRUID, data, sizeof(data));
}

// unwrap system msg, automaticly notify client
int turn_client_unwrap_sys(turn_client *cli, const turn_request *request, turn_sys_status *status, char **commentary, uint32_t *truid){
    if (!cli || !request || !status || !truid || !commentary) return -1;
    if (request->d_size < (sizeof(*truid) + sizeof(status))) return -1;

    size_t offset = 0;
    memcpy((char*)request->data + offset, truid, sizeof(*truid)); offset += sizeof(*truid);
    memcpy((char*)request->data + offset, status, sizeof(*status)); offset += sizeof(*status);
    if (*truid != TURN_SST_FAIL && *truid != TURN_SST_OK){
        fprintf(stderr, "[turn][unwsys] malformed server answer dropped\n");
        return -1;
    }

    if (offset < request->d_size){
        if (request->d_size - offset > TURN_COMMENTARY_MAX_SIZE) return 0;

        *commentary = malloc(request->d_size - offset + 1);
        memcpy(*commentary, (char*)request->data + offset, request->d_size - offset);
        (*commentary)[request->d_size - offset] = '\0';
    }

    prot_table_set(&cli->sys_requests, truid, status);
    mt_evsock_notify(&cli->sys_ev);

    return 0;
}

/*
 * Generates TURN request TURN_OPEN_HOLE
 * returns pckt[turn[TURN_OPEN_HOLE, bind_to info]]
 */ // done
int turn_client_req_bind(turn_client *cli, turn_bind_peer bind_to, protopack **outgoing, uint32_t *out_TRUID){
    if (!cli || !outgoing) return -1;

    turn_request *req = turn_req_make(
        TURN_OPEN_HOLE,
        bind_to.linfo_connected.addr,
        bind_to.linfo_connected.UID,
        cli->TRUID,
        &bind_to.linfo_connected,
        sizeof(light_peer_info)
    );

    if (!req) return -1;

    *outgoing = udp_make_pack(0, cli->turn_sUID, 0, PACK_TURN, req, sizeof(*req));
    *out_TRUID = cli->TRUID++;

    prot_table_set(&cli->sys_requests, out_TRUID, &(turn_sys_status){TURN_SST_SENT});
    return 0;
}

/*
 * Generates TURN request TURN_OPEN_HOLE
 * returns pckt[turn[TURN_OPEN_HOLE, bind_to UID]]
 */ // done
int turn_client_req_unbind(turn_client *cli, turn_bind_peer close_with, protopack **outgoing, uint32_t *out_TRUID){
    if (!cli || !outgoing) return -1;

    turn_request *req = turn_req_make(
        TURN_CLOSE_HOLE,
        close_with.linfo_connected.addr,
        close_with.linfo_connected.UID,
        cli->TRUID,
        NULL, 0
    );

    if (!req) return -1;

    *outgoing = udp_make_pack(0, cli->turn_sUID, 0, PACK_TURN, req, sizeof(*req));
    *out_TRUID = cli->TRUID++;

    prot_table_set(&cli->sys_requests, out_TRUID, &(turn_sys_status){TURN_SST_SENT});
    return 0;
}

int turn_client_req_wait(turn_client *cli, uint32_t TRUID, int timeout_ms, turn_sys_status status){
    int start_ms = mt_time_get_millis_monocoarse();

    while (true){
        int r = mt_evsock_wait(&cli->sys_ev, timeout_ms);
        if (r < 0) return r;

        if (r > 0){
            turn_sys_status *st = prot_table_get(&cli->sys_requests, &TRUID);
            if (st && *st == status)
                return r;
        }

        if (timeout_ms == -1) continue;

        int curr_ms = mt_time_get_millis_monocoarse();
        if (curr_ms - start_ms >= timeout_ms) break;
    }

    return 0;
}

bool turn_check_pair(turn_client *cli, uint32_t uid_from, uint32_t uid_to){
    uint32_t hsh = turn_pair_hash(uid_from, uid_to);

    void *ptr = prot_table_get(&cli->binded_clients, &hsh);
    return ptr != NULL;
}

static uint32_t murmurhash3_32(const char* key, uint32_t len, uint32_t seed) {
    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;
    uint32_t r1 = 15;
    uint32_t r2 = 13;
    uint32_t m = 5;
    uint32_t n = 0xe6546b64;
    uint32_t h = seed;

    const uint32_t* blocks = (const uint32_t*)(key);
    int nblocks = len / 4;

    for (int i = 0; i < nblocks; i++) {
        uint32_t k = blocks[i];
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;

        h ^= k;
        h = (h << r2) | (h >> (32 - r2));
        h = h * m + n;
    }

    const uint8_t* tail = (const uint8_t*)(key + nblocks * 4);
    uint32_t k1 = 0;
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1;
                k1 = (k1 << r1) | (k1 >> (32 - r1));
                k1 *= c2;
                h ^= k1;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

uint32_t turn_pair_hash(uint32_t uid_from, uint32_t uid_to){
    uint8_t data[sizeof(uid_from) * 2];
    memcpy(data, &uid_from, sizeof(uid_from));
    memcpy(data + sizeof(uid_from), &uid_to, sizeof(uid_from));

    return murmurhash3_32((char *)data, sizeof(uid_from) * 2, 0xEAFE);
}
