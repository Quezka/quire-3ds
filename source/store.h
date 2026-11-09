// The account's records as the server sent them, kept on the SD card between runs (read-only copy).
// Shape on disk: {"cursor": "...", "kinds": {"task": {"<uid>": {"modified": "...", "data": {...}}}}}.
#pragma once
#include <stdbool.h>

#include "cJSON.h"

typedef struct {
    cJSON *root;  // owns everything
} Store;

void store_init(Store *s);
void store_free(Store *s);
bool store_load(Store *s, const char *path);
bool store_save(const Store *s, const char *path);
const char *store_cursor(const Store *s);  // "" before the first sync
void store_set_cursor(Store *s, const char *cursor);
// Applies one record from the server: the newest `modified` wins; a deletion leaves a tombstone.
void store_apply(Store *s, const cJSON *record);
// The live (not deleted) records of a kind: an object of uid -> {"modified", "data"}; NULL if none.
cJSON *store_kind(const Store *s, const char *kind);
// The data of one live record, or NULL.
cJSON *store_get(const Store *s, const char *kind, const char *uid);
void store_clear(Store *s);
