#Makefile
#Builds kernel for 32-bit ARM system
#Bryan E. Topp <betopp@betopp.com> 2021

AS = arm-none-eabi-as
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CSRC = $(shell find src -follow -name *.c)
SSRC = $(shell find src -follow -name *.s)

COBJ = $(patsubst src/%.c, obj/c/%.o, $(CSRC))
SOBJ = $(patsubst src/%.s, obj/s/%.o, $(SSRC))

SFLAGS += -march=armv5 -g

CFLAGS += -march=armv5 -mabi=aapcs -ffreestanding -ffixed-r9
CFLAGS += -Wall -Werror -Wextra -pedantic -std=c99
CFLAGS += -I../shared/machine
CFLAGS += -I../../system/libs/mmlibc/include
CFLAGS += -I../../system/libs/libsc/include
CFLAGS += -MMD
CFLAGS += -g

#Build information provided to compiler
BUILDVERSION := $(shell git describe --abbrev=4 --dirty --always --tags)
BUILDDATE := $(shell date -u +%FT%TZ)
BUILDUSER := $(shell whoami)@$(shell hostname)
CFLAGS += -DBUILDVERSION=\"$(BUILDVERSION)\"
CFLAGS += -DBUILDDATE=\"$(BUILDDATE)\"
CFLAGS += -DBUILDUSER=\"$(BUILDUSER)\"

LIBGCC = /usr/local/lib/gcc/arm-none-eabi/8.4.0/libgcc.a

LINKFLAGS = -nostdlib -Tsrc/kernel.ld

#system tar getting linked into kernel
TARFILE=sys.tar
TAROBJ=obj/sys.tar.o

OBJFILES = $(SOBJ) $(COBJ) $(LIBGCC) $(TAROBJ)
ELFFILE=bin/stumparm.elf
BINFILE=bin/stumparm.bin

$(BINFILE) : $(ELFFILE)
	mkdir -p $(@D)
	$(OBJCOPY) -O binary $< $@

$(ELFFILE) : $(OBJFILES)
	mkdir -p $(@D)
	$(CC) $(LINKFLAGS) $^ -o $@

$(TAROBJ) : $(TARFILE)
	mkdir -p $(@D)
	$(OBJCOPY) -I binary -O elf32-littlearm -B armv5 --prefix-sections=.systar $< $@

obj/c/%.o: src/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

obj/s/%.o : src/%.s
	mkdir -p $(@D)
	$(AS) $(SFLAGS) $< -o $@
	
clean:
	rm -rf obj
	rm -rf bin
	
-include $(shell find obj -follow -name *.d)
