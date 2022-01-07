#!/bin/sh
qemu-system-arm -m 128 -kernel bin/stumparm.elf -S -s -machine integratorcp -d guest_errors,unimp,int  

