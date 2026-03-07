#include <crypto/handshaking.h>

int crypto_handshake_server(
    const unsigned char *client_pubk,
    const crypto_keypair *kp, 
    crypto_session_keys  *sk
){
    return crypto_kx_server_session_keys(
        sk->rx, sk->tx, 
        kp->public_key, kp->private_key, 
        client_pubk
    );
}

int crypto_handshake_client(
    const unsigned char *server_pubk,
    const crypto_keypair *kp,
    crypto_session_keys  *sk
){
    return crypto_kx_client_session_keys(
        sk->rx, sk->tx, 
        kp->public_key, kp->private_key, 
        server_pubk
    );
}