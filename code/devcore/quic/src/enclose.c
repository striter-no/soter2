#include <quic/enclose.h>

int enclose_init(enclose_core *core, bool is_server){

    return 0;
}

int enclose_clear(enclose_core *core){
    return 0;
}

int enclose_tunnel_open(enclose_core *core, naddr_t *addr,
                        enclose_proto_t proto, uint64_t *tunnel_id
){
    return 0;
}

int enclose_tunnel_close(enclose_core *core, uint64_t tunnel_id){
    return 0;
}

int enclose_write(enclose_core *core, uint64_t tunnel_id,
                  const uint8_t *data, size_t len
){
    return 0;
}

int enclose_read(enclose_core *core, uint64_t tunnel_id,
                 uint8_t *buf, size_t *len, int timeout
){
    return 0;
}
