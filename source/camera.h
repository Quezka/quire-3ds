// A camera (inner by default, L/R switches to the outer one) for scanning the setup code: a live preview and QR reading.
#pragma once
int camera_start(void);
void camera_stop(void);
// Swaps inner and outer camera (also while scanning).
void camera_switch(void);
// 1 while the inner camera is the one in use.
int camera_inner(void);
// Takes a picture; returns 1 and fills `out` when a QR code was read.
int camera_scan(char *out, int out_size);
// Draws the last picture (grey, coarse: enough to aim) at x, y on the current screen.
void camera_draw(float x, float y);
