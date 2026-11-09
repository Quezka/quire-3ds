#include "link.h"

#include <string.h>

#include "cJSON.h"

static int b64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-' || c == '+') return 62;
    if (c == '_' || c == '/') return 63;
    return -1;
}

int base64url_decode(const char *in, char *out, int out_size) {
    int bits = 0, value = 0, n = 0;
    for (const char *p = in; *p && *p != '='; p++) {
        int v = b64_value(*p);
        if (v < 0) {
            if (*p == '\n' || *p == '\r' || *p == ' ') continue;
            return -1;
        }
        value = (value << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (n >= out_size - 1) return -1;
            out[n++] = (char)((value >> bits) & 0xFF);
        }
    }
    out[n] = 0;
    return n;
}

static bool copy_field(cJSON *json, const char *name, char *dest, size_t size) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(json, name);
    if (!cJSON_IsString(item) || !item->valuestring[0] || strlen(item->valuestring) >= size) return false;
    strcpy(dest, item->valuestring);
    return true;
}

const char *link_parse(const char *text, QuireLink *out) {
    static const char prefix[] = "quire-link:";
    if (strncmp(text, prefix, sizeof prefix - 1) != 0) return "That isn't Quire's setup code.";
    static char raw[4096];
    if (base64url_decode(text + sizeof prefix - 1, raw, sizeof raw) < 0) return "The setup code is damaged. Scan it again.";
    cJSON *json = cJSON_Parse(raw);
    if (!json) return "The setup code is damaged. Scan it again.";
    const char *problem = NULL;
    cJSON *v = cJSON_GetObjectItemCaseSensitive(json, "v");
    cJSON *backend = cJSON_GetObjectItemCaseSensitive(json, "backend");
    if (!cJSON_IsString(v) || strcmp(v->valuestring, "2") != 0 || !cJSON_IsString(backend) ||
        strcmp(backend->valuestring, "server") != 0) {
        problem = "This code is for Firebase sync. Quire for 3DS needs your own Quire server.";
    } else if (!copy_field(json, "url", out->url, sizeof out->url) ||
               !copy_field(json, "refresh", out->refresh, sizeof out->refresh)) {
        problem = "The setup code is missing the server or the sign-in.";
    } else {
        if (!copy_field(json, "email", out->email, sizeof out->email)) out->email[0] = 0;
        size_t len = strlen(out->url);
        while (len && out->url[len - 1] == '/') out->url[--len] = 0;  // no trailing slash
    }
    cJSON_Delete(json);
    return problem;
}
