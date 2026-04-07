#include <soter2/systems.h>
#include <soter2/daemons.h>
#include <uhttp/server.h>
#include <uhttp/parser.h>
#include <multithr/time.h>

#ifndef CLI_HANDLERS_H
#define CLI_HANDLERS_H

// ============ PEERS ============

/*
 * UHTTP handler
 * method: POST
 * path:   /api/connect?ip=...&port=...&pubuid=...
 */
uhttp_response handle_peer_connect(uhttp_high_request *req, void *ctx);

/*
 * UHTTP handler
 * method: POST
 * path:   /api/disconnect?uid=...
 */
uhttp_response handle_peer_disconnect(uhttp_high_request *req, void *ctx);

uhttp_response handle_peer_info(uhttp_high_request *req, void *ctx);

// ============  TURN  ============

uhttp_response handle_turn_connect(uhttp_high_request *req, void *ctx);
uhttp_response handle_turn_disconnect(uhttp_high_request *req, void *ctx);
uhttp_response handle_turn_specs(uhttp_high_request *req, void *ctx);

// =========== STATING ===========

uhttp_response handle_stating_connect(uhttp_high_request *req, void *ctx);
uhttp_response handle_stating_disconnect(uhttp_high_request *req, void *ctx);

// ============ SYSTEM ============

/*
 * UHTTP handler
 * method: GET
 * path:   /api/status
 */
uhttp_response handle_status(uhttp_high_request *req, void *ctx);

uhttp_response handle_wait(uhttp_high_request *req, void *ctx);

#endif
