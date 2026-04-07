#include "soter2/systems.h"
#include "uhttp/api.h"
#include "ujson.h"
#include <uhttp_handlers.h>

uhttp_response handle_status(uhttp_high_request *req, void *ctx) {
    s2_systems *s2s = ctx;

    if (req->req.method != HTTP_GET)
        return uhttp_create_responseq(405, "Method Not Allowed", NULL, 0, HTTP_TEXT_PLAIN);

    ujson_wo *jwo = ujson_wo_create();
    ujson_wo_add_str(jwo, "status", "active");
    ujson_wo_add_str(jwo, "ip", ln_gip(&s2s->io_sys.usock.addr));
    ujson_wo_add_int(jwo, "port", ln_gport(&s2s->io_sys.usock.addr));
    ujson_wo_add_str(jwo, "fingerprint", s2_fingerprint(s2s));

    char *pubuid = s2_pubuid(s2s);
    ujson_wo_add_str(jwo, "pubuid", pubuid);

    char *pjson = NULL;
    if (ujson_wo_serialize(jwo, &pjson, NULL) != UJSON_OK){
        free(pubuid);
        ujson_wo_free(jwo);
        return uhttp_create_responseq(500, "Internal Server Error", "Failed to serialize JSON answer", -1, HTTP_TEXT_PLAIN);
    }

    uhttp_response resp = uhttp_create_responseq(200, "OK", pjson, -1, HTTP_APPLICATION_JSON);
    free(pubuid);
    free(pjson);
    ujson_wo_free(jwo);
    return resp;
}
