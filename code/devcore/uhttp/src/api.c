#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uhttp/api.h>

const char* uhttp_str_content_type(uhttp_content_type ct) {
    switch (ct) {
        case HTTP_TEXT_PLAIN: return "text/plain";

        case HTTP_OCTET_STREAM:
        default: return "application/octet-stream";
    }
}

const char* uhttp_str_method(uhttp_method m) {
    switch (m) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        default: return "UNKNOWN";
    }
}

int uhttp_create_response(
    uhttp_response *out,

    int status_code,
    const char *status_text,

    const void *body,
    size_t body_len,

    uhttp_content_type content_type
){
    if (!out) return -1;

    memset(out, 0, sizeof(uhttp_response));
    out->status_code = status_code;
    out->status_text = status_text ? strdup(status_text): NULL;
    out->body_len = body_len;
    out->content_type = content_type;

    out->headers = NULL;
    out->header_count = 0;

    out->body = NULL;
    if (body && body_len > 0) {
        out->body = malloc(body_len);
        if (!out->body) return -1;
        memcpy(out->body, body, body_len);
    }

    return 0;
}

int uhttp_create_request(
    uhttp_request *out,
    uhttp_method   method,

    const char *path,

    const void *body,
    size_t body_len
){
    if (!out) return -1;
    out->path = path ? strdup(path): NULL;
    if (!path) return -1;

    out->method   = method;
    out->body_len = body_len;
    out->header_count = 0;
    out->headers = NULL;

    out->body = NULL;
    if (body && body_len > 0) {
        out->body = malloc(body_len);
        if (!out->body) {
            free(out->path);
            out->path = NULL;
            return -1;
        }
        memcpy(out->body, body, body_len);
    }

    return 0;
}

int uhttp_request_sheaders(
    uhttp_request *mod,
    uhttp_header  *headers,
    size_t         headers_n
){
    if (!mod) return -1;
    mod->header_count = headers_n;

    if (!headers && !mod->headers) return 0;
    if (!headers && mod->headers){
        free(mod->headers);
        mod->headers = NULL;
        return 0;
    }

    if (headers && !mod->headers){
        mod->headers = malloc(sizeof(uhttp_header) * headers_n);
        memcpy(mod->headers, headers, sizeof(uhttp_header) * headers_n);
        return 0;
    }

    free(mod->headers);
    mod->headers = malloc(sizeof(uhttp_header) * headers_n);
    memcpy(mod->headers, headers, sizeof(uhttp_header) * headers_n);

    return 0;
}

int uhttp_response_sheaders(
    uhttp_response *mod,
    uhttp_header  *headers,
    size_t         headers_n
){
    if (!mod) return -1;
    mod->header_count = headers_n;

    if (!headers && !mod->headers) return 0;
    if (!headers && mod->headers){
        free(mod->headers);
        mod->headers = NULL;
        return 0;
    }

    if (headers && !mod->headers){
        mod->headers = malloc(sizeof(uhttp_header) * headers_n);
        memcpy(mod->headers, headers, sizeof(uhttp_header) * headers_n);
        return 0;
    }

    free(mod->headers);
    mod->headers = malloc(sizeof(uhttp_header) * headers_n);
    memcpy(mod->headers, headers, sizeof(uhttp_header) * headers_n);

    return 0;
}

void uhttp_free_request(uhttp_request *req){
    if (!req) return;

    free(req->path);    req->path = NULL;
    free(req->body);    req->body = NULL;
    free(req->headers); req->headers = NULL;

    req->content_length = 0;
    req->header_count = 0;
    req->body_len = 0;
    req->method = HTTP_METHOD_UNKNOWN;
}

void uhttp_free_response(uhttp_response *resp){
    if (!resp) return;

    free(resp->status_text); resp->status_text = NULL;
    free(resp->body);        resp->body = NULL;
    free(resp->headers);     resp->headers = NULL;

    resp->header_count = 0;
    resp->status_code = -1;
    resp->body_len = 0;
    resp->content_type = HTTP_CONTENT_UNKNOWN;
}

int uhttp_build_response(
    const uhttp_response *resp,
    unsigned char **data,
    size_t         *osz
){
    if (!resp || !data || !osz) return -1;

    const char *ct_str = uhttp_str_content_type(resp->content_type);

    char len_buf[32];
    int len_sz = snprintf(len_buf, sizeof(len_buf), "%zu", resp->body_len);
    if (len_sz < 0) return -1;

    size_t headers_sz = 0;

    headers_sz += strlen("HTTP/1.1 ") + 3 + 1 + strlen(resp->status_text ? resp->status_text : "OK") + strlen("\r\n");
    headers_sz += strlen("Content-Type: ") + strlen(ct_str) + strlen("\r\n");
    headers_sz += strlen("Content-Length: ") + len_sz + strlen("\r\n");
    headers_sz += strlen("Connection: close\r\n");

    for (int i = 0; i < resp->header_count; i++)
        headers_sz += strlen(resp->headers[i].key) + 2 + strlen(resp->headers[i].value) + 2; /* ": \r\n" */

    headers_sz += strlen("\r\n");

    size_t total_sz = headers_sz + resp->body_len;

    *data = malloc(total_sz + 1);
    if (!*data) return -1;

    unsigned char *ptr = *data;

    int written = snprintf((char*)ptr, total_sz + 1,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %s\r\n"
        "Connection: close\r\n",
        resp->status_code,
        resp->status_text ? resp->status_text : "OK",
        ct_str,
        len_buf
    );

    if (written < 0 || (size_t)written > headers_sz) {
        free(*data);
        *data = NULL;
        return -1;
    }

    ptr += written;

    for (int i = 0; i < resp->header_count; i++) {
        int h_written = snprintf((char*)ptr, (total_sz + 1) - (ptr - *data),
            "%s: %s\r\n",
            resp->headers[i].key,
            resp->headers[i].value
        );
        if (h_written < 0) {
            free(*data);
            *data = NULL;
            return -1;
        }
        ptr += h_written;
    }

    memcpy(ptr, "\r\n", 2);
    ptr += 2;

    if (resp->body && resp->body_len > 0)
        memcpy(ptr, resp->body, resp->body_len);

    *osz = total_sz;
    return 0;
}

int uhttp_build_request(
    const uhttp_request *req,
    unsigned char **data,
    size_t         *osz
){
    if (!req || !data || !osz) return -1;

    const char *method_str = uhttp_str_method(req->method);

    char len_buf[32];
    int len_sz = snprintf(len_buf, sizeof(len_buf), "%zu", req->body_len);
    if (len_sz < 0) return -1;

    size_t headers_sz = 0;
    headers_sz += strlen(method_str) + 1 + strlen(req->path) + strlen(" HTTP/1.1\r\n");
    headers_sz += strlen("Content-Length: ") + len_sz + strlen("\r\n");

    for (int i = 0; i < req->header_count; i++)
        headers_sz += strlen(req->headers[i].key) + 2 + strlen(req->headers[i].value) + 2;

    headers_sz += strlen("\r\n");

    size_t total_sz = headers_sz + req->body_len;

    *data = malloc(total_sz + 1);
    if (!*data) return -1;

    unsigned char *ptr = *data;

    int written = snprintf((char*)ptr, total_sz + 1,
        "%s %s HTTP/1.1\r\n"
        "Content-Length: %s\r\n",
        method_str,
        req->path,
        len_buf
    );

    if (written < 0) {
        free(*data);
        *data = NULL;
        return -1;
    }
    ptr += written;

    for (int i = 0; i < req->header_count; i++) {
        int h_written = snprintf((char*)ptr, (total_sz + 1) - (ptr - *data),
            "%s: %s\r\n",
            req->headers[i].key,
            req->headers[i].value
        );
        if (h_written < 0) {
            free(*data);
            *data = NULL;
            return -1;
        }
        ptr += h_written;
    }

    memcpy(ptr, "\r\n", 2);
    ptr += 2;

    if (req->body && req->body_len > 0)
        memcpy(ptr, req->body, req->body_len);

    *osz = total_sz;
    return 0;
}
