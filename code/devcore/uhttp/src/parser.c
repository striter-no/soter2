#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <uhttp/parser.h>

static const unsigned char* find_sequence(
    const unsigned char *buf,
    size_t buf_len,
    const unsigned char *seq,
    size_t seq_len
) {
    if (seq_len > buf_len) return NULL;

    for (size_t i = 0; i <= buf_len - seq_len; i++) {
        if (memcmp(buf + i, seq, seq_len) != 0) continue;

        return buf + i;
    }
    return NULL;
}

static int parse_content_length(const char *str, size_t len, size_t *out) {
    if (!str || len == 0) return -1;

    while (len > 0 && isspace((unsigned char)*str)) {
        str++;
        len--;
    }

    *out = 0;
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)str[i]))
            return -1;
        *out = *out * 10 + (str[i] - '0');

        if (*out > UHTTP_MAX_BODY_SIZE)
            return -1;
    }
    return 0;
}

static int parse_headers(
    const unsigned char *header_start,
    size_t header_len,
    uhttp_header *headers,
    int *header_count,
    size_t *content_length
) {
    *header_count = 0;
    *content_length = 0;

    const unsigned char *line_start = header_start;
    const unsigned char *header_end = header_start + header_len;

    while (line_start < header_end) {
        const unsigned char *line_end = find_sequence(line_start,
            header_end - line_start,
            (const unsigned char*)"\r\n", 2);

        if (!line_end) break;

        const unsigned char *colon = NULL;
        for (const unsigned char *p = line_start; p < line_end; p++) {
            if (*p != ':') continue;

            colon = p;
            break;
        }

        if (!colon) {
            line_start = line_end + 2;
            continue;
        }

        size_t key_len = colon - line_start;
        size_t val_len = line_end - colon - 1;

        while (val_len > 0 && *(colon + 1) == ' ') {
            colon++;
            val_len--;
        }

        if (!(*header_count < MAX_HEADERS &&
            key_len < MAX_HEADER_SIZE &&
            val_len < MAX_HEADER_SIZE)
        ){
            line_start = line_end + 2;
            continue;
        }

        uhttp_header *h = &headers[*header_count];
        memset(h, 0, sizeof(uhttp_header));

        memcpy(h->key, line_start, key_len);
        memcpy(h->value, colon + 1, val_len);

        if ((key_len == 14 && strncasecmp(h->key, "Content-Length", 14) == 0) &&
            parse_content_length(h->value, val_len, content_length) != 0
        ){
            return UHTTP_PARSE_ERROR;
        }

        (*header_count)++;

        line_start = line_end + 2;
    }

    return UHTTP_PARSE_OK;
}

int uhttp_parse_request(
    const unsigned char *data,
    size_t data_size,
    uhttp_request *parsed
) {
    if (!data || !parsed || data_size == 0)
        return UHTTP_PARSE_ERROR;

    memset(parsed, 0, sizeof(uhttp_request));

    const unsigned char *header_end = find_sequence(data, data_size,
        (const unsigned char*)"\r\n\r\n", 4);

    if (!header_end) {
        if (data_size >= UHTTP_MAX_HEADER_SIZE)
            return UHTTP_PARSE_ERROR;

        return UHTTP_PARSE_INCOMPLETE;
    }

    size_t header_len = header_end - data;

    const unsigned char *line_end = find_sequence(data, header_len,
        (const unsigned char*)"\r\n", 2);

    if (!line_end) {
        return UHTTP_PARSE_ERROR;
    }

    const unsigned char *space1 = NULL;
    for (const unsigned char *p = data; p < line_end; p++) {
        if (*p != ' ') continue;

        space1 = p;
        break;
    }

    if (!space1)
        return UHTTP_PARSE_ERROR;

    size_t method_len = space1 - data;
    if (method_len == 3 && memcmp(data, "GET", 3) == 0) {
        parsed->method = HTTP_GET;
    } else if (method_len == 4 && memcmp(data, "POST", 4) == 0) {
        parsed->method = HTTP_POST;
    } else {
        parsed->method = HTTP_METHOD_UNKNOWN;
    }

    const unsigned char *path_start = space1 + 1;
    const unsigned char *space2 = NULL;
    for (const unsigned char *p = path_start; p < line_end; p++) {
        if (*p != ' ') continue;

        space2 = p;
        break;
    }

    if (!space2)
        return UHTTP_PARSE_ERROR;

    size_t path_len = space2 - path_start;
    if (path_len >= MAX_PATH_SIZE)
        return UHTTP_PARSE_ERROR;

    parsed->path = malloc(path_len + 1);
    if (!parsed->path)
        return UHTTP_PARSE_ERROR;

    memcpy(parsed->path, path_start, path_len);
    parsed->path[path_len] = '\0';

    const unsigned char *headers_start = line_end + 2;
    size_t headers_len = header_len - (headers_start - data);

    parsed->headers = calloc(MAX_HEADERS, sizeof(uhttp_header));
    if (!parsed->headers) {
        free(parsed->path);
        return UHTTP_PARSE_ERROR;
    }

    size_t content_length = 0;
    int parse_result = parse_headers(headers_start, headers_len,
        parsed->headers, &parsed->header_count, &content_length);

    if (parse_result != UHTTP_PARSE_OK) {
        free(parsed->path);
        free(parsed->headers);
        return UHTTP_PARSE_ERROR;
    }

    parsed->content_length = content_length;

    const unsigned char *body_start = header_end + 4;
    size_t remaining_data = data_size - (body_start - data);

    if (parsed->method == HTTP_POST && content_length > 0) {
        if (remaining_data < content_length)
            return UHTTP_PARSE_INCOMPLETE;

        if (content_length > UHTTP_MAX_BODY_SIZE)
            return UHTTP_PARSE_ERROR;

        parsed->body = malloc(content_length);
        if (!parsed->body) {
            free(parsed->path);
            free(parsed->headers);
            return UHTTP_PARSE_ERROR;
        }

        memcpy(parsed->body, body_start, content_length);
        parsed->body_len = content_length;
    }

    return UHTTP_PARSE_OK;
}

int uhttp_parse_response(
    const unsigned char *data,
    size_t data_size,

    uhttp_response *parsed
){
    if (!data || !parsed || data_size == 0)
        return UHTTP_PARSE_ERROR;

    memset(parsed, 0, sizeof(uhttp_response));

    const unsigned char *header_end = find_sequence(data, data_size,
        (const unsigned char*)"\r\n\r\n", 4);

    if (!header_end) {
        if (data_size >= UHTTP_MAX_HEADER_SIZE)
            return UHTTP_PARSE_ERROR;

        return UHTTP_PARSE_INCOMPLETE;
    }

    size_t header_len = header_end - data;

    const unsigned char *line_end = find_sequence(data, header_len,
        (const unsigned char*)"\r\n", 2);

    if (!line_end) {
        return UHTTP_PARSE_ERROR;
    }

    const unsigned char *status_start = NULL;
    for (const unsigned char *p = data; p < line_end; p++) {
        if (*p != ' ') continue;

        status_start = p + 1;
        break;
    }

    if (!status_start)
        return UHTTP_PARSE_ERROR;

    const unsigned char *space = NULL;
    for (const unsigned char *p = status_start; p < line_end; p++) {
        if (*p != ' ') continue;

        space = p;
        break;
    }

    if (!space)
        return UHTTP_PARSE_ERROR;

    char code_buf[8] = {0};
    size_t code_len = space - status_start;
    if (code_len >= sizeof(code_buf))
        return UHTTP_PARSE_ERROR;

    memcpy(code_buf, status_start, code_len);
    parsed->status_code = atoi(code_buf);

    const unsigned char *text_start = space + 1;
    size_t text_len = line_end - text_start;

    if (text_len > 0) {
        parsed->status_text = malloc(text_len + 1);
        if (!parsed->status_text)
            return UHTTP_PARSE_ERROR;

        memcpy(parsed->status_text, text_start, text_len);
        parsed->status_text[text_len] = '\0';
    }

    const unsigned char *headers_start = line_end + 2;
    size_t headers_len = header_len - (headers_start - data);

    uhttp_header *headers = calloc(MAX_HEADERS, sizeof(uhttp_header));
    if (!headers) {
        free(parsed->status_text);
        return UHTTP_PARSE_ERROR;
    }

    int header_count = 0;
    size_t content_length = 0;

    int parse_result = parse_headers(headers_start, headers_len,
        headers, &header_count, &content_length);

    parsed->headers = headers;
    parsed->header_count = header_count;

    if (parse_result != UHTTP_PARSE_OK) {
        free(parsed->status_text);
        return UHTTP_PARSE_ERROR;
    }

    const unsigned char *body_start = header_end + 4;
    size_t remaining_data = data_size - (body_start - data);

    if (content_length > 0) {
        if (remaining_data < content_length) {
            return UHTTP_PARSE_INCOMPLETE;
        }

        if (content_length > UHTTP_MAX_BODY_SIZE) {
            free(parsed->status_text);
            return UHTTP_PARSE_ERROR;
        }

        parsed->body = malloc(content_length);
        if (!parsed->body) {
            free(parsed->status_text);
            return UHTTP_PARSE_ERROR;
        }

        memcpy(parsed->body, body_start, content_length);
        parsed->body_len = content_length;
    }

    parsed->content_type = HTTP_OCTET_STREAM; /* По умолчанию */

    return UHTTP_PARSE_OK;
}

int uhttp_get_query_param(const char *query, const char *name, char **out, size_t max_out_size) {
    if (!query || !name || !out || max_out_size == 0 || *name == '\0')
        return -3;

    *out = NULL;

    if (*query == '?') query++;

    size_t name_len = strlen(name);
    const char *p = query;

    while (*p) {
        if (p > query && *(p - 1) != '&') {
            p++;
            continue;
        }

        /* Проверяем имя */
        if (strncmp(p, name, name_len) == 0 && *(p + name_len) == '=') {
            const char *val_start = p + name_len + 1;
            const char *val_end = strchr(val_start, '&');
            if (!val_end) val_end = strchr(val_start, '\0');

            size_t encoded_len = (size_t)(val_end - val_start);
            *out = malloc(max_out_size);
            if (!*out) return -2;

            size_t i = 0;
            const char *src = val_start;
            const char *limit = val_start + encoded_len;

            while (src < limit && i < max_out_size - 1) {
                if (*src == '%') {
                    if (src + 2 < limit &&
                        isxdigit((unsigned char)*(src + 1)) &&
                        isxdigit((unsigned char)*(src + 2))) {

                        /* Быстрый hex -> byte без strtol */
                        char h1 = (char)tolower((unsigned char)*(src + 1));
                        char h2 = (char)tolower((unsigned char)*(src + 2));
                        unsigned char byte = 0;

                        byte |= (h1 >= 'a') ? (h1 - 'a' + 10) : (h1 - '0');
                        byte <<= 4;
                        byte |= (h2 >= 'a') ? (h2 - 'a' + 10) : (h2 - '0');

                        (*out)[i++] = (char)byte;
                        src += 3;
                        continue;
                    }
                } else if (*src == '+') {
                    (*out)[i++] = ' ';
                    src++;
                    continue;
                }
                (*out)[i++] = *src++;
            }
            (*out)[i] = '\0';
            return 0;
        }

        const char *next_amp = strchr(p, '&');
        if (!next_amp) break;
        p = next_amp + 1;
    }

    return -1;
}
