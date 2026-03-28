#ifndef UHTTP_API_H
#define UHTTP_API_H

#include <stddef.h>

#define MAX_HEADERS 20
#define MAX_HEADER_SIZE 256
#define MAX_PATH_SIZE 512

typedef enum {
    HTTP_TEXT_PLAIN,
    HTTP_OCTET_STREAM,
    HTTP_CONTENT_UNKNOWN
} uhttp_content_type;

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_METHOD_UNKNOWN
} uhttp_method;

typedef struct {
    char key  [MAX_HEADER_SIZE];
    char value[MAX_HEADER_SIZE];
} uhttp_header;

typedef struct {
    uhttp_method method;

    char *path;
    uhttp_header *headers;
    int header_count;

    void   *body;
    size_t  body_len;
    size_t  content_length;
} uhttp_request;

typedef struct {
    int status_code;
    char *status_text;

    uhttp_header *headers;
    int header_count;

    void   *body;
    size_t  body_len;

    uhttp_content_type content_type;
} uhttp_response;

int uhttp_create_response(
    uhttp_response *out,

    int status_code,
    const char *status_text,

    const void *body,
    size_t      body_len,

    uhttp_content_type content_type
);

int uhttp_create_request(
    uhttp_request *out,
    uhttp_method   method,

    const char *path,

    const void *body,
    size_t      body_len
);

void uhttp_free_request(uhttp_request *req);
void uhttp_free_response(uhttp_response *req);

int uhttp_request_sheaders(
    uhttp_request *mod,
    uhttp_header  *headers,
    size_t         headers_n
);

int uhttp_response_sheaders(
    uhttp_response *mod,
    uhttp_header  *headers,
    size_t         headers_n
);

int uhttp_build_response(
    const uhttp_response *resp,

    unsigned char **data,
    size_t         *osz
);

int uhttp_build_request(
    const uhttp_request *req,

    unsigned char **data,
    size_t         *osz
);

const char* uhttp_str_content_type(uhttp_content_type ct);
const char* uhttp_str_method(uhttp_method m);

#endif
