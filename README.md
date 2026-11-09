# Quire for 3DS

A read-only window on your [Quire](https://github.com/Quezka/quire) for the Nintendo 3DS: today's plan (lessons,
shifts, events, routines, water), open tasks and your notes, synced from your own quire-server.

## Setting it up

1. Copy `quire.3dsx` to `sd:/3ds/` and start it from the Homebrew Launcher.
2. On your computer, open Quire's sync settings and show the setup code for the phone (it needs sync through your own
   Quire server; Firebase isn't supported here).
3. On the 3DS press **A** and hold the outer camera up to the code.

The sign-in is kept in `sd:/3ds/quire/account.json` and the synced copy in `sd:/3ds/quire/data.json`.
**SELECT** signs out and deletes both.

| Button | Does |
|---|---|
| D-pad / Circle Pad | move, scroll |
| A / B | open a note / back |
| L / R, touch the tabs | Today, Tasks, Notes |
| X | sync now |
| START | quit |

Nothing is ever sent back: change things in Quire on your computer or phone.

## Building

devkitPro with devkitARM, libctru, citro2d, citro3d and `3ds-curl 3ds-mbedtls 3ds-zlib`; put Mozilla's CA bundle at
`romfs/cacert.pem` (`curl -o romfs/cacert.pem https://curl.se/ca/cacert.pem`), then `make`. CI does all of this
(`.github/workflows/build.yml`). The parts that don't need a 3DS have tests on the computer: `make -f tests/Makefile`.

GPL-3.0-only. See THIRD_PARTY_NOTICES.txt.
