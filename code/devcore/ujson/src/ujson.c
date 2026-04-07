#include <ujson.h>
#include <yyjson.h>

ujson_status_t ujson_ro_parse(ujson_ro **out_doc, const char *json_str, size_t len) {
    if (!out_doc || !json_str) return UJSON_ERR_NULL;

    *out_doc = malloc(sizeof(ujson_ro));
    if (!*out_doc) return UJSON_ERR_ALLOC;

    (*out_doc)->doc = yyjson_read(json_str, len, YYJSON_READ_NOFLAG);
    if (!(*out_doc)->doc) {
        free(*out_doc);
        *out_doc = NULL;
        return UJSON_ERR_PARSE;
    }
    return UJSON_OK;
}

ujson_status_t ujson_ro_get_str(const ujson_ro *doc, const char *key, const char **out_val, size_t *out_len) {
    if (!doc || !key || !out_val) return UJSON_ERR_NULL;

    yyjson_val *root = yyjson_doc_get_root(doc->doc);
    yyjson_val *val = yyjson_obj_get(root, key);
    if (!val || !yyjson_is_str(val)) return UJSON_ERR_TYPE;

    *out_val = yyjson_get_str(val);
    if (out_len) *out_len = yyjson_get_len(val);
    return UJSON_OK;
}

void ujson_ro_free(ujson_ro *doc) {
    if (doc) {
        if (doc->doc) yyjson_doc_free(doc->doc);
        free(doc);
    }
}

// -- writing
ujson_wo* ujson_wo_create(void) {
    ujson_wo *wo = malloc(sizeof(ujson_wo));
    if (!wo) return NULL;

    wo->doc = yyjson_mut_doc_new(NULL);
    if (!wo->doc) {
        free(wo);
        return NULL;
    }

    yyjson_mut_val *root = yyjson_mut_obj(wo->doc);
    yyjson_mut_doc_set_root(wo->doc, root);

    return wo;
}

ujson_status_t ujson_wo_add_str(ujson_wo *doc, const char *key, const char *val) {
    if (!doc || !key || !val || !doc->doc) return UJSON_ERR_NULL;

    yyjson_mut_val *k = yyjson_mut_str(doc->doc, key);
    yyjson_mut_val *v = yyjson_mut_str(doc->doc, val);
    if (!k || !v) return UJSON_ERR_ALLOC;

    yyjson_mut_obj_add(doc->doc->root, k, v);
    return UJSON_OK;
}

ujson_status_t ujson_wo_add_int(ujson_wo *doc, const char *key, int64_t val) {
    if (!doc || !key || !doc->doc) return UJSON_ERR_NULL;

    yyjson_mut_val *k = yyjson_mut_str(doc->doc, key);
    yyjson_mut_val *v = yyjson_mut_int(doc->doc, val);
    if (!k || !v) return UJSON_ERR_ALLOC;

    yyjson_mut_obj_add(doc->doc->root, k, v);
    return UJSON_OK;
}

ujson_status_t ujson_wo_serialize(const ujson_wo *doc, char **out_json, size_t *out_len) {
    if (!doc || !out_json || !doc->doc) return UJSON_ERR_NULL;

    *out_json = yyjson_mut_write(doc->doc, YYJSON_WRITE_NOFLAG, NULL);
    if (!*out_json) return UJSON_ERR_WRITE;
    if (out_len) *out_len = strlen(*out_json);
    return UJSON_OK;
}

void ujson_wo_free(ujson_wo *doc) {
    if (doc) {
        if (doc->doc) yyjson_mut_doc_free(doc->doc);
        free(doc);
    }
}
