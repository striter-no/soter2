#include <npunch/stun.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>

#define STUN_BINDING_REQUEST  0x0001
#define STUN_BINDING_RESPONSE 0x0101
#define STUN_MAGIC_COOKIE     0x2112A442
#define ATTR_XOR_MAPPED_ADDR  0x0020


struct __attribute__((packed)) stun_header {
    uint16_t type;
    uint16_t length;
    uint32_t magic;
    uint8_t  id[12];
};

struct stun_attr {
    uint16_t type;
    uint16_t length;
};

int natp_request_stun(
    ln_usocket *client,
    naddr_t  stun_addr
){
    struct stun_header req;
    req.type   = htons(STUN_BINDING_REQUEST);
    req.length = htons(0);
    req.magic  = htonl(STUN_MAGIC_COOKIE);
    
    for(int i=0; i<12; i++) req.id[i] = (uint8_t)(rand() & 0xFF);

    nnet_fd stun_fd = ln_netfdq(stun_addr);
    ssize_t s = sendto(client->fd.rfd, &req, sizeof(req), 0, 
                       (struct sockaddr*)&stun_fd.addr, stun_fd.addr_len);
    if (s != sizeof(req)){
        perror("sendto");
        return -1;
    }
    // fprintf(stderr, "[STUN] sent %zd bytes to %s:%d\n", s, 
    //         stun_addr.ip.v4.ip, stun_addr.ip.v4.port);

    uint8_t buf[1024] = {0};
    struct sockaddr_storage from;
    socklen_t from_len = sizeof(from);
    
    // Проверяем результат ожидания
    int wait_r = ln_wait_netfd(client->fd, POLLIN, 1000); // 1 сек достаточно
    if (wait_r <= 0) {
        fprintf(stderr, "[STUN] wait timeout/error (r=%d)\n", wait_r);
        return -1; 
    }
    
    int r = recvfrom(client->fd.rfd, buf, sizeof(buf), 0, 
                     (struct sockaddr*)&from, &from_len);
    
    if (r < 0) {
        perror("[STUN] recvfrom");
        return -1;
    }
    
    if (r < (int)sizeof(struct stun_header)) {
        fprintf(stderr, "[STUN] response too small: %d bytes\n", r);
        return -1;
    }

    struct stun_header *res = (struct stun_header *)buf;
    
    // fprintf(stderr, "[STUN] type=0x%04x, magic=0x%08x\n", 
    //         ntohs(res->type), ntohl(res->magic));
    
    if (ntohs(res->type) != STUN_BINDING_RESPONSE) {
        fprintf(stderr, "[STUN] wrong type: expected 0x%04x, got 0x%04x\n",
                STUN_BINDING_RESPONSE, ntohs(res->type));
        return -1;
    }
    
    if (ntohl(res->magic) != STUN_MAGIC_COOKIE) {
        fprintf(stderr, "[STUN] wrong magic cookie\n");
        return -1;
    }

    uint16_t msg_len = ntohs(res->length);
    size_t pos = sizeof(struct stun_header);
    size_t end_pos = pos + msg_len;

    if (end_pos > (size_t)r) {
        end_pos = (size_t)r;
    }

    while (pos + 4 <= end_pos) {
        struct stun_attr *attr = (struct stun_attr *)&buf[pos];
        uint16_t type = ntohs(attr->type);
        uint16_t len  = ntohs(attr->length);

        // fprintf(stderr, "[STUN] attr type=0x%04x, len=%u\n", type, len);

        if (type == ATTR_XOR_MAPPED_ADDRESS || type == ATTR_XOR_MAPPED_ADDRESS_RFC5389) {
            if (len < 8) {
                fprintf(stderr, "[STUN] XOR_MAPPED_ADDR too short\n");
                return -1;
            }

            uint16_t x_port = 0;
            uint32_t x_ip = 0;
            
            memcpy(&x_port, &buf[pos + 4 + 2], 2);
            memcpy(&x_ip, &buf[pos + 4 + 4], 4);

            uint16_t port = ntohs(x_port) ^ (STUN_MAGIC_COOKIE >> 16);
            uint32_t ip   = ntohl(x_ip) ^ STUN_MAGIC_COOKIE;

            struct in_addr in;
            in.s_addr = htonl(ip);
            
            // fprintf(stderr, "[STUN] XOR discovered: %s:%u\n", inet_ntoa(in), port);
            client->addr = ln_make4(ln_ipv4(inet_ntoa(in), port));
            return 0;
        }
        
        if (type == ATTR_MAPPED_ADDRESS) {
            if (len < 8) {
                fprintf(stderr, "[STUN] MAPPED_ADDR too short\n");
                return -1;
            }

            uint8_t family = buf[pos + 4 + 1];
            if (family != 0x01) {
                fprintf(stderr, "[STUN] Only IPv4 supported\n");
                return -1;
            }

            uint16_t port = 0;
            uint32_t ip = 0;
            
            memcpy(&port, &buf[pos + 4 + 2], 2);
            memcpy(&ip, &buf[pos + 4 + 4], 4);

            uint16_t real_port = ntohs(port);
            uint32_t real_ip = ntohl(ip);

            struct in_addr in;
            in.s_addr = htonl(real_ip);
            
            // fprintf(stderr, "[STUN] MAP discovered: %s:%u\n", inet_ntoa(in), real_port);
            client->addr = ln_make4(ln_ipv4(inet_ntoa(in), real_port));
            return 0;
        }

        pos += 4 + ((len + 3) & ~3);
    }

    fprintf(stderr, "[STUN] no XOR_MAPPED_ADDR attribute found\n");
    return -1;
}