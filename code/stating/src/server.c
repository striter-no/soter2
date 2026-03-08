#include <stating/state.h>

int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr, "usage: %s [bind IP] [bind PORT]\n", argv[0]);
        return 0;
    }

    ln_usocket sock;
    ln_usock_new(&sock);
    ln_usock_bind(&sock, ln_make4(ln_ipv4(argv[1], atoi(argv[2]))));
    
    

    ln_usock_close(&sock);
}