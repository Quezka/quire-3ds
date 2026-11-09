"""Writes setup codes like the desktop's, and a camera-sized grey picture of one as a QR, for the host tests."""
import base64
import json
import sys

import segno


def link(url, refresh):
    raw = json.dumps({"v": "2", "backend": "server", "url": url, "email": "a@b.c", "refresh": refresh},
                     separators=(",", ":")).encode()
    return "quire-link:" + base64.urlsafe_b64encode(raw).decode().rstrip("=")


out = sys.argv[1]
open(f"{out}/code-example.txt", "w").write(link("https://sync.example.com/", "r3fr3sh"))
code = link("https://sync.example.com", "x" * 86)
open(f"{out}/code.txt", "w").write(code)
m = [list(r) for r in segno.make(code, error="m").matrix]
n, scale, border, W, H = len(m), 3, 4, 400, 240
size = (n + 2 * border) * scale
ox, oy = (W - size) // 2, (H - size) // 2
img = bytearray([200] * (W * H))
for y in range(size):
    for x in range(size):
        mx, my = x // scale - border, y // scale - border
        img[(oy + y) * W + ox + x] = 25 if 0 <= mx < n and 0 <= my < n and m[my][mx] else 235
open(f"{out}/code.pgm", "wb").write(b"P5 %d %d 255\n" % (W, H) + bytes(img))
