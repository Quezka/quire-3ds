# Host tests (no 3DS needed):  make -f tests/Makefile
CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -O1
OUT = build-tests
SRC = source/link.c source/store.c source/agenda.c source/cJSON.c
QR = source/qrdecode.c source/quirc.c source/decode.c source/identify.c source/version_db.c

all:
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/test tests/test.c $(SRC)
	$(CC) $(CFLAGS) -o $(OUT)/test_qr tests/test_qr.c $(QR) -lm
	python3 tests/make_code.py $(OUT)
	$(OUT)/test "$$(cat $(OUT)/code-example.txt)"
	$(OUT)/test_qr $(OUT)/code.pgm "$$(cat $(OUT)/code.txt)"
