// Finds a QR code in a grey picture (one byte per pixel) and copies its text. Returns 1 when found.
#pragma once
int qr_decode(const unsigned char *grey, int width, int height, char *out, int out_size);
