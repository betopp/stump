#!/bin/sh


gdb \
	-ex "set confirm off" \
	-ex "target remote localhost:1234" \
	-ex "symbol-file bin/stump64.elf" \
	-ex "set confirm on"

