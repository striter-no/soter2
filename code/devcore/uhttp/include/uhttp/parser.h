#ifndef UHTTP_PARSER_H
#define UHTTP_PARSER_H

#include "api.h"

#define UHTTP_PARSE_OK 0
#define UHTTP_PARSE_ERROR -1
#define UHTTP_PARSE_INCOMPLETE 1

#define UHTTP_MAX_HEADER_SIZE 8192
#define UHTTP_MAX_BODY_SIZE (10 * 1024 * 1024)

int uhttp_parse_response(
    const unsigned char *data,
    size_t data_size,

    uhttp_response *parsed
);

int uhttp_parse_request(
    const unsigned char *data,
    size_t data_size,

    uhttp_request *parsed
);

#endif
