#!/bin/sh

qemu-system-x86_64 -m 512 -kernel bin/stump64.bin -s -S --accel tcg,thread=single -smp 4 -cpu max -d guest_errors,int  -vga std



