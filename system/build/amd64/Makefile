#Makefile for AMD64 system tar
#Bryan E. Topp <betopp@betopp.com> 2021

#Input directories
P = ../../progs
L = ../../libs

#Output directories
B = bin
O = obj
A = lib

#Toolchain location
CC = gcc
AS = nasm
AR = ar

#Assembler flags
ASFLAGS += -f elf64
ASFLAGS += -g -F dwarf

#Compile flags
CFLAGS += -m64 -march=athlon64 -mabi=sysv -ffreestanding 
CFLAGS += -std=c99 -Wall -Werror -Wextra -pedantic -Wshadow
CFLAGS += -nostdlib -nostdinc
CFLAGS += -I$(L)/mmlibc/include
CFLAGS += -I$(L)/libsc/include
CFLAGS += -I$(L)/pcmd/include
CFLAGS += -g -mno-sse -O1
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections

#Build information provided to compiler
BUILDVERSION := $(shell git describe --abbrev=4 --dirty --always --tags)
BUILDDATE := $(shell date -u +%FT%TZ)
BUILDUSER := $(shell whoami)@$(shell hostname)
CFLAGS += -DBUILDVERSION=\"$(BUILDVERSION)\"
CFLAGS += -DBUILDDATE=\"$(BUILDDATE)\"
CFLAGS += -DBUILDUSER=\"$(BUILDUSER)\"

#Link flags
LDFLAGS += -nostdlib -Tcrtx/user.ld
LDFLAGS += -static
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -z max-page-size=0x1000

#Standard compiler invocations
PRELINK = $(O)/crt0.o $(O)/crti.o
POSTLINK = $(O)/crtn.o
STDLIBS = $(A)/pcmd.a $(A)/mmlibc.a $(A)/libsc.a /usr/lib/libgcc.a

CC_ELF = mkdir -p $(@D) && $(CC) $(LDFLAGS) $^ -o $@
CC_OBJ = mkdir -p $(@D) && $(CC) $(CFLAGS) -c $< -o $@ -MMD
AR_A = mkdir -p $(@D) && $(AR) -r $@ $^

#tarball containing stuff that is copied verbatim into the system tar (device nodes, mainly)
TEMPLATE=../../template/template.tar

#Top level target - tar file (additional deps defined in per-program and per-library Makefiles)
sys.tar: $(TEMPLATE) $(PRELINK) $(POSTLINK)
	mkdir -p $(@D)
	cp $(TEMPLATE) $@
	tar  --exclude $(TEMPLATE)  -rf $@ $^

#How to build each part
include $(wildcard $(L)/*/Makefile)
include $(wildcard $(P)/*/Makefile)

#C runtime libraries
$(O)/crt%.o : crtx/crt%.asm
	mkdir -p $(@D)
	$(AS) $(ASFLAGS) $< -o $@
	
clean: 
	rm -rf $(O)
	rm -rf $(B)
	rm -rf $(A)
	rm -f sys.tar
	
-include $(shell find $(O) -name *.d)
