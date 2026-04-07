#include <soter2/systems.h>
#include <soter2/daemons.h>
#include <uhttp/server.h>
#include <uhttp/parser.h>
#include <multithr/time.h>

#ifndef CLI_HANDLERS_H
#define CLI_HANDLERS_H

/*
 * UHTTP handler
 * method: POST
 * path:   /api/connect?ip=...&port=...&pubuid=...
 */
uhttp_response handle_connect(uhttp_high_request *req, void *ctx);

/*
 * UHTTP handler
 * method: POST
 * path:   /api/disconnect?uid=...
 */
uhttp_response handle_disconnect(uhttp_high_request *req, void *ctx);

/*
 * UHTTP handler
 * method: GET
 * path:   /api/status
 */
uhttp_response handle_status(uhttp_high_request *req, void *ctx);

#endif
