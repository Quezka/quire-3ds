#include "qrdecode.h"

#include <string.h>

#include "quirc.h"

int qr_decode(const unsigned char *grey, int width, int height, char *out, int out_size) {
    static struct quirc *q = NULL;
    static int qw = 0, qh = 0;
    if (!q) q = quirc_new();
    if (!q) return 0;
    if (qw != width || qh != height) {
        if (quirc_resize(q, width, height) < 0) return 0;
        qw = width;
        qh = height;
    }
    int w, h;
    uint8_t *image = quirc_begin(q, &w, &h);
    memcpy(image, grey, (size_t)w * h);
    quirc_end(q);
    int found = 0;
    for (int i = 0; i < quirc_count(q) && !found; i++) {
        struct quirc_code code;
        struct quirc_data data;
        quirc_extract(q, i, &code);
        if (quirc_decode(&code, &data) == QUIRC_SUCCESS && data.payload_len < out_size) {
            memcpy(out, data.payload, data.payload_len);
            out[data.payload_len] = 0;
            found = 1;
        }
    }
    return found;
}
