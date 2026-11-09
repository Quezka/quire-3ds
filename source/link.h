// The phone setup code the desktop shows as a QR: "quire-link:" + base64url(JSON).
// Quire for 3DS only speaks to a quire-server (version 2 codes), never Firebase.
#pragma once
#include <stdbool.h>

typedef struct {
    char url[256];      // the server's address, e.g. https://sync.example.com
    char email[128];
    char refresh[512];  // the long-lived refresh token: the only secret kept
} QuireLink;

// Fills `out` from a scanned code. Returns NULL when it worked, else a message for the person.
const char *link_parse(const char *text, QuireLink *out);

// Decodes base64url (no padding needed) into `out` (NUL-terminated). Returns the length, or -1.
int base64url_decode(const char *in, char *out, int out_size);
