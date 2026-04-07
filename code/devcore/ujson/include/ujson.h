#ifndef UJSON_H
#define UJSON_H

#include <stddef.h>
#include <stdint.h>
#include <yyjson.h>

typedef struct {
    yyjson_doc *doc;
} ujson_ro;

typedef struct {
    yyjson_mut_doc *doc;
} ujson_wo;

typedef enum {
    UJSON_OK          =  0,
    UJSON_ERR_NULL    = -1,
    UJSON_ERR_PARSE   = -2,
    UJSON_ERR_NOT_FOUND = -3,
    UJSON_ERR_TYPE    = -4,
    UJSON_ERR_ALLOC   = -5,
    UJSON_ERR_WRITE   = -6
} ujson_status_t;

// immutable, read-only
ujson_status_t ujson_ro_parse(ujson_ro **out_doc, const char *json_str, size_t len);
ujson_status_t ujson_ro_get_str(const ujson_ro *doc, const char *key, const char **out_val, size_t *out_len);
ujson_status_t ujson_ro_get_int(const ujson_ro *doc, const char *key, int64_t *out_val);
void           ujson_ro_free(ujson_ro *doc);

// mutable, write-only
ujson_wo*    ujson_wo_create(void);
ujson_status_t ujson_wo_add_str(ujson_wo *doc, const char *key, const char *val);
ujson_status_t ujson_wo_add_int(ujson_wo *doc, const char *key, int64_t val);
ujson_status_t ujson_wo_serialize(const ujson_wo *doc, char **out_json, size_t *out_len);
void           ujson_wo_free(ujson_wo *doc);

#endif
