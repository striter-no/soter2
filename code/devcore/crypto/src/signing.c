#include <crypto/signing.h>

sign sign_gen(){
    sign sgn;
    crypto_sign_keypair(sgn.id_pub, sgn.id_sec);

    return sgn;
}

int sign_data(
    const sign *sgn, // just for id_sec (no id_pub used)
    const unsigned char *msg_to_sign,
    size_t         msg_len,
    unsigned char  out_signature[crypto_sign_BYTES]
){
    return crypto_sign_detached(
        out_signature, NULL, 
        msg_to_sign, msg_len, 
        sgn->id_sec
    );
}

int sign_verify(
    const unsigned char *msg_signed,
    size_t         msg_len,
    const unsigned char  in_signature[crypto_sign_BYTES],
    const unsigned char  pub_key     [crypto_sign_PUBLICKEYBYTES]
){
    return crypto_sign_verify_detached(in_signature, msg_signed, msg_len, pub_key);
}

int sign_store(const sign *kp, const char *path){
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    if (1 != fwrite(kp, sizeof(*kp), 1, f)){
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

int sign_load(sign *kp, const char *path){
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    if (fread(kp, sizeof(*kp), 1, f) != 1){
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 0;
}