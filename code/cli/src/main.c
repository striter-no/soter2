#include <soter2/systems.h>
#include <soter2/daemons.h>
#include <uhttp_handlers.h>
#include <signal.h>

uhttp_serv http_serv;

static void signal_handler(int sig) {
    (void)sig;
    printf("\n[main] received signal, shutting down...\n");
    printf("[main] stopping HTTP server...\n");
    uhttp_server_stop(&http_serv);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s PATH [HTTP_PORT]\n\nPATH - path to cryptographic signature\nHTTP_PORT - port for HTTP server (default: 8080)\n", argv[0]);
        return 1;
    }

    const char *sign_path = argv[1];
    unsigned short http_port = 8080;
    if (argc >= 3) {
        http_port = (unsigned short)atoi(argv[2]);
    }

    srand(mt_time_get_seconds_monocoarse());

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    s2_systems s2s;
    s2_daemons s2d;

    if (s2_systems_create(&s2s) < 0) {
        fprintf(stderr, "[main] failed to create systems\n");
        return -1;
    }

    if (s2_sys_upd_sign(&s2s, sign_path) < 0) {
        fprintf(stderr, "[main] failed to load/save signature from/to \"%s\"\n", sign_path);
        s2_systems_stop(&s2s);
        return -1;
    }

    char *pubuid = s2_pubuid(&s2s);
    printf("[main] my info:\n");
    printf("[main]   uid:         %u\n", s2s.crypto_mod.UID);
    printf("[main]   fingerprint: %s\n", s2_fingerprint(&s2s));
    printf("[main]   pubuid:      %s\n", pubuid);
    free(pubuid);

    // STUN resolution
    stun_addr stuns[] = {
        {"stun.sipnet.ru", 3478},
        {"stun.ekiga.net", 3478}
    };

    nat_type nt = NAT_ERROR;
    for (int i = 0; i < 5 && nt == NAT_ERROR; i++) {
        nt = s2_sys_stun_perf(&s2s, stuns, sizeof(stuns) / sizeof(stuns[0]));
        if (nt == NAT_ERROR) sleep(1);
    }

    if (nt == NAT_ERROR) {
        fprintf(stderr, "[main] failed to get NAT type\n");
        s2_systems_stop(&s2s);
        return -1;
    }

    printf("[main]   global ip:   %s:%u\n", ln_gip(&s2s.io_sys.usock.addr), ln_gport(&s2s.io_sys.usock.addr));
    printf("[main] NAT type: %s\n", strnattype(nt));

    if (s2_systems_run(&s2s) < 0) {
        fprintf(stderr, "[main] failed to run systems\n");
        s2_systems_stop(&s2s);
        return -1;
    }

    if (s2_daemons_create(&s2d, &s2s) < 0){
        fprintf(stderr, "[main] failed to create system daemons\n");
        s2_systems_stop(&s2s);
        return -1;
    }

    printf("[main] systems started\n");

    uhttp_serv http_serv;
    naddr_t http_addr;
    ln_uni("0.0.0.0", http_port, &http_addr);

    if (uhttp_server_create(&http_serv, http_addr, &s2s) < 0) {
        fprintf(stderr, "[main] failed to create HTTP server\n");
        s2_systems_stop(&s2s);
        return -1;
    }

    uhttp_set_route(&http_serv, "/api/peer/connect",    handle_peer_connect); // done
    uhttp_set_route(&http_serv, "/api/peer/disconnect", handle_peer_disconnect); // done
    uhttp_set_route(&http_serv, "/api/peer/info",       handle_peer_info);

    uhttp_set_route(&http_serv, "/api/turn/connect",    handle_turn_connect);
    uhttp_set_route(&http_serv, "/api/turn/disconnect", handle_turn_disconnect);
    uhttp_set_route(&http_serv, "/api/turn/specs",      handle_turn_specs);

    uhttp_set_route(&http_serv, "/api/stating/connect",    handle_stating_connect);
    uhttp_set_route(&http_serv, "/api/stating/disconnect", handle_stating_disconnect);

    uhttp_set_route(&http_serv, "/api/status", handle_status); // done
    uhttp_set_route(&http_serv, "/api/wait",   handle_wait);

    printf("[main] HTTP server listening on port %u\n", http_port);
    printf("[main] API endpoints:\n");
    printf("[main]   POST /api/connect?uni=...&port=...&pubuid=...\n");
    printf("[main]   POST /api/disconnect?uid=...\n");
    printf("[main]   GET  /api/status\n");

    uhttp_server_poll_routes(&http_serv);

    printf("[main] stopping HTTP server...\n");
    uhttp_server_stop(&http_serv);
    uhttp_server_end(&http_serv);

    printf("[main] stopping daemons...\n");
    s2_daemons_stop(&s2d);

    printf("[main] stopping systems...\n");
    s2_systems_stop(&s2s);

    printf("[main] done\n");
    return 0;
}
