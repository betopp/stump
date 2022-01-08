#Makefile fragment for system program
#Bryan E. Topp <betopp@betopp.com> 2021

PROGDIR := linuxdoom-1.10
PROGVAR := LINUXDOOM

$(PROGVAR)_SRC := $(P)/$(PROGDIR)/src
$(PROGVAR)_OBJ := $(O)/$(PROGDIR)
$(PROGVAR)_BIN := $(B)/$(PROGDIR)

$(PROGVAR)_CSRC := $(shell find $($(PROGVAR)_SRC) -name *.c)
$(PROGVAR)_COBJ := $(patsubst $($(PROGVAR)_SRC)/%.c, $($(PROGVAR)_OBJ)/%.o, $($(PROGVAR)_CSRC))

sys.tar: $($(PROGVAR)_BIN)

$($(PROGVAR)_BIN): $(PRELINK) $($(PROGVAR)_COBJ) $(STDLIBS) $(POSTLINK)
	$(CC_ELF)
	
$($(PROGVAR)_OBJ)/%.o : $($(PROGVAR)_SRC)/%.c
	$(CC_OBJ) 



$($(PROGVAR)_OBJ)/%.o : CFLAGS += -Wno-format-overflow
$($(PROGVAR)_OBJ)/%.o : CFLAGS += -Wno-sizeof-pointer-memaccess
$($(PROGVAR)_OBJ)/%.o : CFLAGS += -Wno-format