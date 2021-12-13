#Makefile fragment for system program
#Bryan E. Topp <betopp@betopp.com> 2021

PROGDIR := sterm
PROGVAR := STERM

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


