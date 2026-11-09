#include "sync.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

typedef struct {
    char *data;
    size_t size;
} Buffer;

static size_t collect(void *chunk, size_t size, size_t count, void *user) {
    Buffer *b = user;
    size_t n = size * count;
    char *grown = realloc(b->data, b->size + n + 1);
    if (!grown) return 0;
    b->data = grown;
    memcpy(b->data + b->size, chunk, n);
    b->size += n;
    b->data[b->size] = 0;
    return n;
}

// One HTTPS call. Returns the HTTP status (0 when the network failed); the body lands in `out`.
static long request(const char *url, const char *bearer, const char *post_json, const char *ca_file, Buffer *out) {
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    struct curl_slist *headers = curl_slist_append(NULL, "Accept: application/json");
    char auth[700];
    if (bearer) {
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", bearer);
        headers = curl_slist_append(headers, auth);
    }
    if (post_json) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_json);
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Quire-3DS/0.1");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    if (ca_file) curl_easy_setopt(curl, CURLOPT_CAINFO, ca_file);
    long status = 0;
    if (curl_easy_perform(curl) == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return status;
}

// The server's own words for an error, when it gave some.
static const char *server_message(const Buffer *b, const char *fallback) {
    static char message[200];
    cJSON *json = b->data ? cJSON_Parse(b->data) : NULL;
    cJSON *error = cJSON_GetObjectItemCaseSensitive(json, "error");
    cJSON *text = cJSON_GetObjectItemCaseSensitive(error, "message");
    snprintf(message, sizeof message, "%s", cJSON_IsString(text) ? text->valuestring : fallback);
    cJSON_Delete(json);
    return message;
}

const char *sync_pull(const QuireLink *link, Store *store, const char *ca_file, SyncProgress progress, void *context) {
    char url[512];
    Buffer body = {0};
    cJSON *refresh = cJSON_CreateObject();
    cJSON_AddStringToObject(refresh, "refresh_token", link->refresh);
    char *refresh_json = cJSON_PrintUnformatted(refresh);
    cJSON_Delete(refresh);
    snprintf(url, sizeof url, "%s/v1/refresh", link->url);
    long status = request(url, NULL, refresh_json, ca_file, &body);
    free(refresh_json);
    if (status == 0) {
        free(body.data);
        return "Can't reach your Quire server. Check the Wi-Fi.";
    }
    if (status != 200) {
        const char *message = server_message(&body, "Sign in again: scan a new setup code from Quire on your computer.");
        free(body.data);
        return message;
    }
    cJSON *session = cJSON_Parse(body.data);
    free(body.data);
    cJSON *token_item = cJSON_GetObjectItemCaseSensitive(session, "token");
    if (!cJSON_IsString(token_item)) {
        cJSON_Delete(session);
        return "The server sent an answer Quire doesn't understand.";
    }
    char token[600];
    snprintf(token, sizeof token, "%s", token_item->valuestring);
    cJSON_Delete(session);

    int total = 0;
    for (int page = 0; page < 10000; page++) {
        const char *cursor = store_cursor(store);
        char *escaped = NULL;
        if (cursor[0]) {
            CURL *c = curl_easy_init();
            escaped = c ? curl_easy_escape(c, cursor, 0) : NULL;
            if (c) curl_easy_cleanup(c);
        }
        snprintf(url, sizeof url, "%s/v1/records?limit=500%s%s", link->url, escaped ? "&after=" : "", escaped ? escaped : "");
        if (escaped) curl_free(escaped);
        Buffer page_body = {0};
        status = request(url, token, NULL, ca_file, &page_body);
        if (status != 200) {
            const char *message = status ? server_message(&page_body, "The server refused to send your data.")
                                         : "The connection dropped. What arrived is kept; press X to carry on.";
            free(page_body.data);
            return message;
        }
        cJSON *answer = cJSON_Parse(page_body.data);
        free(page_body.data);
        cJSON *records = cJSON_GetObjectItemCaseSensitive(answer, "records");
        cJSON *next = cJSON_GetObjectItemCaseSensitive(answer, "cursor");
        if (!cJSON_IsArray(records) || !cJSON_IsString(next)) {
            cJSON_Delete(answer);
            return "The server sent an answer Quire doesn't understand.";
        }
        const cJSON *r;
        cJSON_ArrayForEach(r, records) {
            store_apply(store, r);
            total++;
        }
        store_set_cursor(store, next->valuestring);
        int more = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "more"));
        cJSON_Delete(answer);
        if (progress) progress(total, context);
        if (!more) break;
    }
    return NULL;
}
