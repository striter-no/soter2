#include <soter2/interface.h>
#include <stdio.h>

int main(){
    srand(time(NULL));

    soter2_interface intr;
    if (0 > soter2_intr_init(&intr, rand() % UINT32_MAX)) {
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
    soter2_iconnect(&intr, ln_make4(ln_ipv4(ip, port)), uid);
    soter2_istatewait(&intr, uid, PEER_ST_ACTIVE, &info);
    soter2_inew_chan(&intr, info.nfd, uid);

    rudp_channel *chan = soter2_iget_chan(&intr, uid);
    for (int i = 0; i < 10; i++){
        char data[8]; snprintf(data, 8, "Hello %d", i);
        soter2_isend(&intr, chan, data, strlen(data));

        if (0 >= soter2_iwait_pack(chan, SOTER_DEAD_DT * 1000)){
            break;
        }

        protopack *r = soter2_irecv(chan);
        printf("> %.*s\n", r->d_size, r->data);
        free(r);
    }

    // flushing incoming packets
    if (0 < soter2_iwait_pack(chan, 2000)){
        protopack *r = soter2_irecv(chan);
        printf("> %.*s\n", r->d_size, r->data);
        free(r);
    }

    soter2_intr_end(&intr);
}