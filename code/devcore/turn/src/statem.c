#include <turn/statem.h>
#include <turn/packets.h>

int turn_client_init(turn_client *cli, uint32_t turn_sUID){
    if (!cli) return -1;

    if (0 > mt_evsock_new(&cli->outgoing_ev))
        return -1;

    if (0 > prot_queue_create(sizeof(protopack*), &cli->outgoing_tpacks))
        return -1;

    if (0 > prot_table_create(sizeof(uint32_t), sizeof(turn_bind_peer), DYN_OWN_BOTH, &cli->binded_clients))
        return -1;

    cli->turn_sUID = turn_sUID;

    return 0;
}

int turn_client_end (turn_client *cli){
    if (!cli) return -1;

    mt_evsock_close(&cli->outgoing_ev);
    prot_queue_end(&cli->outgoing_tpacks);
    prot_table_end(&cli->binded_clients);

    return 0;
}

int turn_client_bind(turn_client *cli, uint32_t client_UID, turn_bind_peer *peer){
    if (!cli || !peer) return -1;

    prot_table_set(&cli->binded_clients, &client_UID, peer);

    return 0;
}

int turn_client_unbind(turn_client *cli, uint32_t client_UID){
    if  (!cli) return -1;

    prot_table_remove(&cli->binded_clients, &client_UID);
    return 0;
}

int turn_client_wrap(
    turn_client *cli,
    protopack *pack,

    protopack **outgoing
){
    if (!cli || !pack || !outgoing) return -1;

    // check if user exists
    turn_bind_peer *bpeer = prot_table_get(&cli->binded_clients, &pack->h_to);
    if (!bpeer) return -1;

    char data[2048] = {0};
    size_t totsize = protopack_send(pack, data);

    // if so, wrap packet to outgoing
    turn_request *req = turn_req_make(TURN_DATA_MSG, bpeer->linfo.addr, data, totsize);
    if (!req) return -1;

    *outgoing = udp_make_pack(
        0, cli->turn_sUID, pack->h_to, PACK_TURN, req, sizeof(*req) + req->d_size
    );

    free(req);
    return 0;
}

int turn_client_unwrap(turn_client *cli, protopack *pack, protopack **outgoing){
    if (!cli || !pack || !outgoing) return -1;

    // check if user exists
    turn_bind_peer *bpeer = prot_table_get(&cli->binded_clients, &pack->h_to);
    if (!bpeer) return -1;

    turn_request *req = turn_req_recv(pack->data, pack->d_size);
    if (!req) return -1;

    *outgoing = protopack_recv((char*)req->data, req->d_size);
    if (!*outgoing) return -1;

    free(req);
    return 0;
}

int turn_client_req_bind(turn_client *cli, turn_bind_peer bind_to, protopack **outgoing){
    if (!cli || !outgoing) return -1;

    turn_request *req = turn_req_make(TURN_OPEN_HOLE, bind_to.linfo.addr, &bind_to.linfo, sizeof(light_peer_info));
    if (!req) return -1;

    *outgoing = udp_make_pack(
        0, cli->turn_sUID, 0, PACK_TURN, req, sizeof(*req)
    );

    return 0;
}

int turn_client_req_unbind(turn_client *cli, turn_bind_peer close_with, protopack **outgoing){
    if (!cli || !outgoing) return -1;

    turn_request *req = turn_req_make(TURN_CLOSE_HOLE, close_with.linfo.addr, &close_with.linfo, sizeof(light_peer_info));
    if (!req) return -1;

    *outgoing = udp_make_pack(
        0, cli->turn_sUID, 0, PACK_TURN, req, sizeof(*req)
    );

    return 0;
}
