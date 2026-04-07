#include "soter2/systems.h"
#include "uhttp/api.h"
#include <uhttp_handlers.h>

static int _actual_handler(s2_systems *s2s, uint32_t uid){
    return s2_sys_closeconn(s2s, uid);
}

uhttp_response handle_peer_disconnect(uhttp_high_request *req, void *ctx) {
    s2_systems *s2s = ctx;

    if (req->req.method != HTTP_POST)
        return uhttp_create_responseq(405, "Method Not Allowed", NULL, 0, HTTP_TEXT_PLAIN);

    if (!uhttp_any_params(&req->req))
        return uhttp_create_responseq(400, "Bad request", "No query params", -1, HTTP_TEXT_PLAIN);

    uhttp_param params[] = {uhttp_new_param("uid")};
    if (uhttp_parse_params(&req->req, params, 1) != 0){
        uhttp_free_params(params, 1);
        return uhttp_create_responseq(400, "Bad Request", "Missing required parameters: uid", -1, HTTP_TEXT_PLAIN);
    }

    uint32_t uid = atoi(uhttp_get_param(params, 3, "uid"));
    uhttp_free_params(params, 1);

    if (_actual_handler(s2s, uid) < 0)
        return uhttp_create_responseq(404, "Not Found", "Peer not found", -1, HTTP_TEXT_PLAIN);

    char fmted[512];
    snprintf(fmted, sizeof(fmted), "Disconnected from peer %u", uid);

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
