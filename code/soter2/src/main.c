#include <soter2/modules.h>
#include <soter2/handlers.h>

#include <unistd.h>

typedef struct {
    rudp_dispatcher *rudp;
    watcher         *watcher;
    peers_db        *p_db;
    gossip_system   *g_syst;
} app_context;

typedef struct {
    naddr_t  addr;
    uint32_t UID;
} app_peer_info;

static void cycle( app_context ctx );
int main(){
    // -- udp net socket
    ln_usocket sock;
    if (0 > ln_usock_new(&sock)) return -1;
    
    // auto bind
    nat_type nt = nat_get_type(
        &sock,
        ln_resolveq("stun.sipnet.ru", 3478),
        ln_resolveq("stun.ekiga.net", 3478),
        63000
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
    if (0 > watcher_init(&wtch, &sender)) return -1;

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

    cycle((app_context){
        .g_syst = &gsyst,
        .p_db = &pdb,
        .watcher = &wtch,
        .rudp = &rudp_disp
    });

    // Ending all systems
    rudp_dispatcher_end(&rudp_disp);

    gossip_system_end(&gsyst);
    peers_db_end(&pdb);

    watcher_end(&wtch);
    pvd_sender_end(&sender);
    pvd_listener_end(&listener);
    
    ln_usock_close(&sock);
}

static void punch(app_context ctx, app_peer_info peer){
    ;
}

static void cycle(app_context ctx){
    // non-RUDP first
    watcher_handler_reg(ctx.watcher, PACK_ACK,    (watcher_handler){soter2_hnd_ACK,    NULL});
    watcher_handler_reg(ctx.watcher, PACK_PUNCH,  (watcher_handler){soter2_hnd_PUNCH,  NULL});
    watcher_handler_reg(ctx.watcher, PACK_PING,   (watcher_handler){soter2_hnd_PING,   NULL});
    watcher_handler_reg(ctx.watcher, PACK_PONG,   (watcher_handler){soter2_hnd_PONG,   NULL});
    watcher_handler_reg(ctx.watcher, PACK_GOSSIP, (watcher_handler){soter2_hnd_GOSSIP, NULL});
    watcher_handler_reg(ctx.watcher, PACK_STATE,  (watcher_handler){soter2_hnd_STATE,  NULL});

    watcher_handler_reg(ctx.watcher, PACK_DATA,   (watcher_handler){soter2_hnd_DATA,   NULL});
    watcher_handler_reg(ctx.watcher, PACK_RACK,   (watcher_handler){soter2_hnd_RACK,   NULL});
    watcher_handler_reg(ctx.watcher, PACK_HELLO,  (watcher_handler){soter2_hnd_HELLO,  NULL});
    watcher_handler_reg(ctx.watcher, PACK_REJECT, (watcher_handler){soter2_hnd_REJECT, NULL});
    watcher_handler_reg(ctx.watcher, PACK_ACCEPT, (watcher_handler){soter2_hnd_ACCEPT, NULL});
    
    while (atomic_load(&ctx.watcher->is_running)){
        ;
    }
}