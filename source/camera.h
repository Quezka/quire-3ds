// The outer camera, for scanning the setup code: a live preview and QR reading.
#pragma once
int camera_start(void);
void camera_stop(void);
// Takes a picture; returns 1 and fills `out` when a QR code was read.
int camera_scan(char *out, int out_size);
// Draws the last picture (grey, coarse: enough to aim) at x, y on the current screen.
void camera_draw(float x, float y);
