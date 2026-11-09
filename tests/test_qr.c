// Decodes a PGM picture of a QR code with the same code the 3DS uses.  ./test_qr file.pgm expected-text
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../source/qrdecode.h"

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    int w, h, max;
    assert(fscanf(f, "P5 %d %d %d", &w, &h, &max) == 3);
    fgetc(f);
    unsigned char *grey = malloc(w * h);
    assert(fread(grey, 1, w * h, f) == (size_t)(w * h));
    fclose(f);
    char text[4096];
    assert(qr_decode(grey, w, h, text, sizeof text));
    assert(strcmp(text, argv[2]) == 0);
    puts("qr decoded");
    return 0;
}
