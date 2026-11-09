# Quire for 3DS

C (gnu11) homebrew for the Nintendo 3DS on devkitPro (libctru, citro2d, 3ds-curl/mbedTLS). Read-only Quire client for a
quire-server (protocol: ../quire-server/docs/PROTOCOL.md). Started 2026-11-09.

- Pure C, tested on the computer (`make -f tests/Makefile`, needs `segno`): `link.c` (the `quire-link:` setup code,
  version 2 only), `store.c` (records on the SD card, newest `modified` wins, tombstones), `agenda.c` (today, tasks,
  notes; weekday 0 = Monday like the desktop), `qrdecode.c` (quirc), `sync.c` (curl; refresh then pull pages; never
  pushes). 3DS only: `camera.c` (outer camera, grey preview drawn as squares), `main.c` (screens, input).
- This computer has no devkitPro (installing needs root; devkitPro's servers refuse non-pacman downloads): CI builds
  the `.3dsx` in the `devkitpro/devkitarm` container. A tag `vX.Y.Z` publishes a release (CHANGELOG section as notes).
- `tests/test_sync.c` runs against a local quire-server (QUIRE_DB/QUIRE_SECRET/QUIRE_PORT, `add-user`, push records)
  and links the system libcurl (headers fetched from curl's repo; this computer has no libcurl-dev).
- Vendored: cJSON 1.7.18 (MIT), quirc (ISC). Big, high-contrast text: the owner doesn't see well.
- Nothing tested on a real 3DS by Claude: camera orientation, TLS to the server and the system font's coverage of
  Russian are unconfirmed.
