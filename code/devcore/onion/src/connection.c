#include <onion/connection.h>

int onion_connection_new(
    onion_connection *conn,
    rele_circut *circ,
    onion_peer req_by,
    peers_db *pdb
){
    if (!conn) return -1;

    // we are not owner of the circut
    conn->requested_by = req_by;
    conn->next_hop = (onion_peer){UINT32_MAX, (nnet_fd){0}};
    conn->pdb = pdb;

    return 0;
}

int onion_connection_free(onion_connection *conn){
    if (!conn) return -1;

    conn->requested_by = (onion_peer){UINT32_MAX, (nnet_fd){0}};
    conn->next_hop = (onion_peer){UINT32_MAX, (nnet_fd){0}};

    return 0;
}

int onion_connection_updcirc(
    onion_connection *conn,
    peer_info *snapshot,
    size_t snapshot_sz,

    int nodes_n,

    void *ctx,
    uint32_t (*next_uid_provider)(dyn_array *excl, void *ctx)
){
    if (!conn || !snapshot || snapshot_sz == 0 || nodes_n == 0) return -1;

    dyn_array excl = dyn_array_create(sizeof(uint32_t));
    for (int i = 0; i < nodes_n; i++) {
        uint32_t uid = next_uid_provider(&excl, ctx);
        if (uid == UINT32_MAX) {
            fprintf(stderr, "[onion][error] failed to find a suitable peer for circut generation\n");
            dyn_array_end(&excl);
            return -1;
        }

        peer_info info;
        if (0 > peers_db_get(conn->pdb, uid, &info)){
            fprintf(stderr, "[onion][error] failed to get peer info for circut generation\n");
            dyn_array_end(&excl);
            return -1;
        }

        dyn_array_push(&excl, &uid);
        rele_circut_extend(&conn->circut, &info);

        // if it first connection or no
        // unsigned char zero_sec[64] = {0};
        // if (memcmp(conn->my_conn.st_signature.id_sec, zero_sec, sizeof(zero_sec)) == 0){
        //     ;
        // }
    }

    dyn_array_end(&excl);

    return 0;
}

int onion_connection_forward(
    onion_connection *conn,
    const onion_packet *pkt,
    onion_packet *forwarded,
    bool         *no_forwarding
){
    if (!conn) return -1;

    // pkt will be encrypted by our keys
    e2ee_encrypt()

    // if we are endpoint, no forwarding appears
    *no_forwarding = conn->next_hop.uid == UINT32_MAX;
    return 0;
}

int onion_connection_backprop(
    onion_connection *conn,
    const onion_packet *pkt,
    onion_packet *backproped,
    bool         *no_backprop
){
    if (!conn) return -1;

    return 0;
}

int onion_connection_extend(
    onion_connection *conn,
    onion_peer new_peer
){
    // generate new e2ee connection
    if (!conn) return -1;

    return 0;
}
