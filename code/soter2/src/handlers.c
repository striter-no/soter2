#include <soter2/handlers.h>

int soter2_hnd_ACK(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx){
    
}

int soter2_hnd_PUNCH(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx){
    
}


int soter2_hnd_PING  (protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
int soter2_hnd_PONG  (protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
int soter2_hnd_GOSSIP(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
int soter2_hnd_STATE (protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);