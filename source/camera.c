#include "camera.h"

#include <3ds.h>
#include <citro2d.h>
#include <stdlib.h>
#include <string.h>

#include "qrdecode.h"

#define W 400
#define H 240
#define BLOCK 5

static u16 *pixels;
static u8 *grey;
static int running;
static u32 transfer;

int camera_start(void) {
    if (running) return 1;
    pixels = linearAlloc(W * H * 2);
    grey = malloc(W * H);
    if (!pixels || !grey || R_FAILED(camInit())) {
        if (pixels) linearFree(pixels);
        free(grey);
        pixels = NULL;
        grey = NULL;
        return 0;
    }
    CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A);
    CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A);
    CAMU_SetNoiseFilter(SELECT_OUT1, true);
    CAMU_SetAutoExposure(SELECT_OUT1, true);
    CAMU_SetAutoWhiteBalance(SELECT_OUT1, true);
    CAMU_SetTrimming(PORT_CAM1, false);
    CAMU_GetMaxBytes(&transfer, W, H);
    CAMU_SetTransferBytes(PORT_CAM1, transfer, W, H);
    CAMU_Activate(SELECT_OUT1);
    CAMU_ClearBuffer(PORT_CAM1);
    CAMU_StartCapture(PORT_CAM1);
    memset(grey, 0, W * H);
    running = 1;
    return 1;
}

void camera_stop(void) {
    if (!running) return;
    CAMU_StopCapture(PORT_CAM1);
    CAMU_Activate(SELECT_NONE);
    camExit();
    linearFree(pixels);
    free(grey);
    pixels = NULL;
    grey = NULL;
    running = 0;
}

int camera_scan(char *out, int out_size) {
    if (!running) return 0;
    Handle event = 0;
    CAMU_SetReceiving(&event, pixels, PORT_CAM1, W * H * 2, (s16)transfer);
    Result r = svcWaitSynchronization(event, 300000000ULL);
    svcCloseHandle(event);
    if (R_FAILED(r)) return 0;
    for (int i = 0; i < W * H; i++) {
        u16 p = pixels[i];
        int red = (p >> 11) & 0x1F, green = (p >> 5) & 0x3F, blue = p & 0x1F;
        grey[i] = (u8)((red * 8 * 77 + green * 4 * 150 + blue * 8 * 29) >> 8);
    }
    return qr_decode(grey, W, H, out, out_size);
}

void camera_draw(float x, float y) {
    if (!grey) return;
    for (int by = 0; by < H; by += BLOCK) {
        for (int bx = 0; bx < W; bx += BLOCK) {
            u8 v = grey[(by + BLOCK / 2) * W + bx + BLOCK / 2];
            C2D_DrawRectSolid(x + bx, y + by, 0.1f, BLOCK, BLOCK, C2D_Color32(v, v, v, 0xFF));
        }
    }
}
