#include "store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fresh(Store *s) {
    s->root = cJSON_CreateObject();
    cJSON_AddStringToObject(s->root, "cursor", "");
    cJSON_AddObjectToObject(s->root, "kinds");
}

void store_init(Store *s) { fresh(s); }

void store_free(Store *s) {
    cJSON_Delete(s->root);
    s->root = NULL;
}

void store_clear(Store *s) {
    store_free(s);
    fresh(s);
}

bool store_load(Store *s, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc(size + 1);
    if (!text) {
        fclose(f);
        return false;
    }
    size_t got = fread(text, 1, size, f);
    fclose(f);
    text[got] = 0;
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root || !cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(root, "kinds"))) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_Delete(s->root);
    s->root = root;
    return true;
}

bool store_save(const Store *s, const char *path) {
    char *text = cJSON_PrintUnformatted(s->root);
    if (!text) return false;
    char temp[512];
    snprintf(temp, sizeof temp, "%s.part", path);
    FILE *f = fopen(temp, "wb");
    bool ok = f && fwrite(text, 1, strlen(text), f) == strlen(text);
    if (f) fclose(f);
    free(text);
    // Written aside, then swapped in, so a power cut never leaves half a file.
    if (ok) {
        remove(path);
        ok = rename(temp, path) == 0;
    }
    return ok;
}

const char *store_cursor(const Store *s) {
    cJSON *c = cJSON_GetObjectItemCaseSensitive(s->root, "cursor");
    return cJSON_IsString(c) ? c->valuestring : "";
}

void store_set_cursor(Store *s, const char *cursor) {
    cJSON_ReplaceItemInObjectCaseSensitive(s->root, "cursor", cJSON_CreateString(cursor ? cursor : ""));
}

static cJSON *kinds(const Store *s) { return cJSON_GetObjectItemCaseSensitive(s->root, "kinds"); }

void store_apply(Store *s, const cJSON *record) {
    const cJSON *kind = cJSON_GetObjectItemCaseSensitive(record, "kind");
    const cJSON *uid = cJSON_GetObjectItemCaseSensitive(record, "uid");
    const cJSON *modified = cJSON_GetObjectItemCaseSensitive(record, "modified");
    const cJSON *deleted = cJSON_GetObjectItemCaseSensitive(record, "deleted");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(record, "data");
    if (!cJSON_IsString(kind) || !cJSON_IsString(uid) || !cJSON_IsString(modified)) return;
    cJSON *bucket = cJSON_GetObjectItemCaseSensitive(kinds(s), kind->valuestring);
    if (!bucket) bucket = cJSON_AddObjectToObject(kinds(s), kind->valuestring);
    cJSON *old = cJSON_GetObjectItemCaseSensitive(bucket, uid->valuestring);
    if (old) {
        cJSON *old_modified = cJSON_GetObjectItemCaseSensitive(old, "modified");
        // ISO-8601 UTC with milliseconds compares correctly as text.
        if (cJSON_IsString(old_modified) && strcmp(old_modified->valuestring, modified->valuestring) > 0) return;
    }
    cJSON *entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "modified", modified->valuestring);
    if (cJSON_IsTrue(deleted) || !cJSON_IsObject(data)) {
        cJSON_AddTrueToObject(entry, "deleted");
    } else {
        cJSON_AddItemToObject(entry, "data", cJSON_Duplicate(data, true));
    }
    if (old) {
        cJSON_ReplaceItemInObjectCaseSensitive(bucket, uid->valuestring, entry);
    } else {
        cJSON_AddItemToObject(bucket, uid->valuestring, entry);
    }
}

cJSON *store_kind(const Store *s, const char *kind) { return cJSON_GetObjectItemCaseSensitive(kinds(s), kind); }

cJSON *store_get(const Store *s, const char *kind, const char *uid) {
    if (!uid) return NULL;
    cJSON *bucket = store_kind(s, kind);
    cJSON *entry = bucket ? cJSON_GetObjectItemCaseSensitive(bucket, uid) : NULL;
    return entry ? cJSON_GetObjectItemCaseSensitive(entry, "data") : NULL;
}
