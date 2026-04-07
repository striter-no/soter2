#include "soter2/systems.h"
#include "uhttp/api.h"
#include "ujson.h"
#include <uhttp_handlers.h>

uhttp_response handle_connect(uhttp_high_request *req, void *ctx){
    printf("[handle_connect] called\n");
    s2_systems *s2s = ctx;

    if (req->req.method != HTTP_POST) {
        return uhttp_create_responseq(405, "Method not allowed", NULL, 0, HTTP_TEXT_PLAIN);
    }

    if (!uhttp_any_params(&req->req))
        return uhttp_create_responseq(400, "Bad request", "No query params", -1, HTTP_TEXT_PLAIN);

    uhttp_param params[] = {uhttp_new_param("uni"), uhttp_new_param("port"), uhttp_new_param("pubuid")};

    if (uhttp_parse_params(&req->req, params, 3) != 0){
        uhttp_free_params(params, 3);
        return uhttp_create_responseq(400, "Bad Request", "Missing required parameters: uni, port, pubuid", -1, HTTP_TEXT_PLAIN);
    }

    const char *uni_pm    = uhttp_get_param(params, 3, "uni");
    const char *port_pm   = uhttp_get_param(params, 3, "port");
    const char *pubuid_pm = uhttp_get_param(params, 3, "pubuid");

    uint32_t UID = 0;
    unsigned char PUBKEY[CRYPTO_PUBKEY_BYTES] = {0};
    unsigned short port = (unsigned short)atoi(port_pm);
    if (0 > crypto_decode_pubkey_uid(pubuid_pm, PUBKEY, &UID)){
        uhttp_free_params(params, 3);
        return uhttp_create_responseq(400, "Bad Request", "Failed to decode pubuid to pubkey and uid", -1, HTTP_TEXT_PLAIN);
    }

    naddr_t addr, serv_addr = {0};
    if (ln_uni(uni_pm, port, &addr) < 0){
        uhttp_free_params(params, 3);
        return uhttp_create_responseq(400, "Bad Request", "Invalid IP address/Domain", -1, HTTP_TEXT_PLAIN);
    }

    if (0 > s2_sys_new_peer(s2s, addr, serv_addr, UID, PUBKEY)){
        uhttp_free_params(params, 3);
        return uhttp_create_responseq(500, "Internal Server Error", "Failed to add peer", -1, HTTP_TEXT_PLAIN);
    }

    if (0 > s2_sys_nat_punch(s2s, UID)){
        uhttp_free_params(params, 3);
        peers_db_remove(&s2s->peers_sys, UID);
        return uhttp_create_responseq(500, "Internal Server Error", "Failed to NAT punch", -1, HTTP_TEXT_PLAIN);
    }

    char fmted[512];
    snprintf(fmted, sizeof(fmted), "Connecting to peer %u at %s:%u", UID, uni_pm, port);
    uhttp_free_params(params, 3);

    ujson_wo *jwo = ujson_wo_create();
    ujson_wo_add_str(jwo, "status", "ok");
    ujson_wo_add_str(jwo, "message", fmted);

    char *jresp = NULL;
    if (ujson_wo_serialize(jwo, &jresp, NULL) != UJSON_OK){
        ujson_wo_free(jwo);
        return uhttp_create_responseq(500, "Failed to serialize JSON response", NULL, 0, HTTP_TEXT_PLAIN);
    }

    uhttp_response resp = uhttp_create_responseq(200, "OK", jresp, -1, HTTP_APPLICATION_JSON);
    free(jresp);
    ujson_wo_free(jwo);

    return resp;
}
