#Makefile for 64-bit kernel on AMD64
#Bryan E. Topp <betopp@betopp.com> 2021

NASM=nasm
OBJCOPY=objcopy
LD=ld
CC=gcc

SRCDIR=src
OBJDIR=obj
BINDIR=bin

ELFFILE=$(BINDIR)/stump64.elf
BINFILE=$(BINDIR)/stump64.bin

LDSCRIPT=$(SRCDIR)/kernel.ld

NASMFLAGS += -f elf64
NASMFLAGS += -g -F dwarf

CFLAGS += -ffreestanding -nostdinc
CFLAGS += -mcmodel=kernel -mabi=sysv -mno-red-zone
CFLAGS += -std=gnu99 -Wall -Werror -Wextra -pedantic -Wshadow
CFLAGS += -I../../system/libs/mmlibc/include
CFLAGS += -I../../system/libs/libsc/include
CFLAGS += -I../shared/machine
CFLAGS += -g -O1 -mno-sse -mno-avx

LDFLAGS += -g
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -nostdlib

NASMSRC = $(shell find $(SRCDIR)/ -follow -name *.asm)
NASMOBJ = $(patsubst $(SRCDIR)/%.asm, $(OBJDIR)/s/%.o, $(NASMSRC))

CSRC = $(shell find $(SRCDIR)/ -follow -name *.c)
COBJ = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/c/%.o, $(CSRC))

#TAR file gets turned into an object with objcopy and linked into its own section
TARFILE=sys.tar
TAROBJ=obj/sys.tar.o

$(BINFILE) : $(ELFFILE)
	mkdir -p $(@D)
	$(OBJCOPY) -O binary $< $@
	
$(ELFFILE) : $(NASMOBJ) $(COBJ) $(TAROBJ)
	mkdir -p $(@D)
	$(LD) $(LDFLAGS) $^ -o $@
	
$(TAROBJ) : $(TARFILE)
	mkdir -p $(@D)
	$(OBJCOPY) -I binary -O elf64-x86-64 -B amd64 --prefix-sections=.systar $< $@
	
$(OBJDIR)/s/%.o : $(SRCDIR)/%.asm
	mkdir -p $(@D)
	$(NASM) $(NASMFLAGS) $< -o $@
	
$(OBJDIR)/c/%.o : $(SRCDIR)/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD

clean:
	rm -rf obj/ bin/


-include $(shell find $(OBJDIR)/ -follow -name *.d)
