#include <soter2/modules.h>
#include <soter2/handlers.h>

#include <stdio.h>
#include <unistd.h>

static void* passer(void *_args);
static void cycle( app_context ctx );
static void punch( app_context ctx, app_peer_info peer );
int main(){
    srand(time(NULL));

    // -- udp net socket
    ln_usocket sock;
    if (0 > ln_usock_new(&sock)) return -1;
    
    // auto bind
    nat_type nt = nat_get_type(
        &sock,
        ln_resolveq("stun.sipnet.ru", 3478),
        ln_resolveq("stun.ekiga.net", 3478),
        1024 + (rand() % 63000)
    );

    uint32_t self_uid = rand() % UINT32_MAX;
    printf("Current NAT type: %s\n", strnattype(nt));
    printf("My address: %s:%u:%u\n", sock.addr.ip.v4.ip, sock.addr.ip.v4.port, self_uid);

    // -- providers
    pvd_listener listener;
    pvd_sender   sender;

    if (0 > pvd_listener_new(&listener, &sock)) return -1;
    if (0 > pvd_sender_new  (&sender, &sock))   return -1;

    // -- watcher
    watcher wtch;
    if (0 > watcher_init(&wtch, &sender, &listener)) return -1;

    // -- rudp

    rudp_dispatcher rudp_disp;
    rudp_dispatcher_new(&rudp_disp, &sender, self_uid);

    // -- databases

    peers_db pdb;
    gossip_system gsyst;
    peers_db_init(&pdb);
    gossip_system_init(&gsyst);

    // -- running
    pvd_listener_start(&listener);
    pvd_sender_start(&sender);
    watcher_start(&wtch);
    rudp_dispatcher_run(&rudp_disp);

    app_context ctx = {
        .g_syst   = &gsyst,
        .p_db     = &pdb,
        .watcher  = &wtch,
        .rudp     = &rudp_disp,
        .listener = &listener
    };

    pthread_t passer_daemon;
    pthread_create(&passer_daemon, NULL, passer, &ctx);
    
    cycle(ctx);
    watcher_end(&wtch);
    
    pthread_join(passer_daemon, NULL);

    // Ending all systems
    rudp_dispatcher_end(&rudp_disp);

    gossip_system_end(&gsyst);
    peers_db_end(&pdb);

    pvd_sender_end(&sender);
    pvd_listener_end(&listener);
    
    ln_usock_close(&sock);
}

static void punch(app_context ctx, app_peer_info peer){
    printf("[main] punch sended\n");
    pvd_sender_send(
        ctx.watcher->sender,
        proto_msg_quick(ctx.rudp->self_uid, peer.UID, 0, PACK_PUNCH),
        ln_netfdq(peer.addr)
    );
}

static void cycle(app_context ctx){
    // non-RUDP first
    watcher_handler_reg(ctx.watcher, PACK_ACK,    (watcher_handler){soter2_hnd_ACK,    &ctx});
    watcher_handler_reg(ctx.watcher, PACK_PUNCH,  (watcher_handler){soter2_hnd_PUNCH,  &ctx});
    watcher_handler_reg(ctx.watcher, PACK_PING,   (watcher_handler){soter2_hnd_PING,   &ctx});
    watcher_handler_reg(ctx.watcher, PACK_PONG,   (watcher_handler){soter2_hnd_PONG,   &ctx});
    watcher_handler_reg(ctx.watcher, PACK_GOSSIP, (watcher_handler){soter2_hnd_GOSSIP, &ctx});
    watcher_handler_reg(ctx.watcher, PACK_STATE,  (watcher_handler){soter2_hnd_STATE,  &ctx});
    
    char ip[INET_ADDRSTRLEN]; 
    unsigned port, uid;
    printf("[main] ip:port:uid > "); fflush(stdin);
    scanf("%[^:]:%u:%u", ip, &port, &uid);

    peers_db_add(ctx.p_db, (peer_info){
        .last_seen = time(NULL),
        .UID = uid,
        .state = PEER_ST_INITED,
        .nfd = ln_netfdq(ln_make4(ln_ipv4(ip, port))),
        .ctx = NULL // wth am I supposed to pass here
    });

    punch(ctx, (app_peer_info){
        .addr = ln_make4(ln_ipv4(ip, port)),
        .UID = uid
    });

    printf("[main] waiting peer\n");
    peer_info info;
    if (0 > peers_db_wait(ctx.p_db, uid, PEER_ST_ACTIVE, &info)){
        fprintf(stderr, "[main] failed to wait peer from PeersDB\n");
        return;
    }

    naddr_t addr = ln_nfd2addr(info.nfd);
    printf("[main] got new peer: %s:%u\n", addr.ip.v4.ip, addr.ip.v4.port);


    protopack *p = udp_make_pack(0, ctx.rudp->self_uid, info.UID, PACK_DATA, "Hello", 5);
    rudp_dispatcher_send(ctx.rudp, p, info.nfd);
    
    protopack *r = NULL;
    rudp_dispatcher_chan_wait(ctx.rudp, -1);
    
    rudp_channel *channel = NULL;
    rudp_dispatcher_chan_get(ctx.rudp, info.UID, &channel);
    rudp_channel_wait(channel, -1);
    rudp_channel_recv(channel, &r);

    printf("got RUDP pack: %.*s\n", r->d_size, r->data);
    free(r);
}

static void* passer(void *_args){
    app_context *ctx = _args;

    while (atomic_load(&ctx->watcher->is_running)){
        int r = mt_evsock_wait(&ctx->listener->newpack_es, 100);
        if (r == 0) continue;
        if (r < 0) {
            perror("poll()");
            continue;
        }

        listener_packet pkt = pvd_next_packet(ctx->listener);
        if (pkt.pack == 0 || pkt.from_who.addr_len == 0) continue;

        if (udp_is_RUDP_req(pkt.pack->packtype)){
            rudp_dispatcher_pass(ctx->rudp, pkt.pack, pkt.from_who);
        } else {
            watcher_pass(ctx->watcher, pkt.pack, pkt.from_who);
        }
    }

    return 0;
}