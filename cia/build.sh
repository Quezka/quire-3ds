#!/bin/sh
# Builds quire.cia from the built quire.elf (needs makerom and bannertool on PATH).
set -e
cd "$(dirname "$0")/.."
bannertool makebanner -i cia/banner.png -a cia/silent.wav -o cia/banner.bnr
bannertool makesmdh -s Quire -l "Your day, tasks and notes from Quire" -p Quezka -i icon.png -o cia/icon.smdh
makerom -f cia -o quire.cia -elf quire.elf -rsf cia/app.rsf -target t -exefslogo -icon cia/icon.smdh -banner cia/banner.bnr
